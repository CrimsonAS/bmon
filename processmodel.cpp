#include <QDirIterator>
#include <QDebug>
#include <QTimer>
#include <QProcess>

#include "processmodel.h"

void ProcessInfo::update()
{
    bool ok = true;

    ok = ok && gatherStatus();

    setLive(ok);

    calculatePowerImpact();
}

void ProcessInfo::setLive(bool l)
{
    if (l == m_live) {
        if (l == false && !m_reallyDead) {
            m_reallyDead = true;
            QTimer::singleShot(2000, this, [=](){
                emit processDead();
            });
        }
        return;
    }

    m_live = l;
    emit liveChanged();
}

bool ProcessInfo::gatherStatus()
{
    QFile f(QString::fromLatin1("/proc/%1/status").arg(pid()));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Can't gather sched: " << f.errorString();
        return false;
    }

    QByteArray contents = f.readAll();
    QList<QByteArray> entries = contents.split('\n');
    QMap<QString, QString> kv;

    for (const QByteArray &entry : entries) {
        QList<QByteArray> entrySplit = entry.split(':');

        if (entrySplit.size() < 2) {
            continue; // just in case
        }

        kv[entrySplit[0]] = entrySplit[1].trimmed();
    }

    QString name = kv["Name"];
    if (name != m_name) {
        m_name = name;
        emit nameChanged();
    }

    m_ppid = kv["PPid"].toInt();

    m_lastnrVoluntarySwitches = m_nrVoluntarySwitches;
    m_nrVoluntarySwitches = kv["voluntary_ctxt_switches"].toInt();
    if (m_lastnrVoluntarySwitches == 0) {
        m_lastnrVoluntarySwitches = m_nrVoluntarySwitches;
    }
    emit nrVoluntarySwitchesChanged();

    m_lastnrInvoluntarySwitches = m_nrInvoluntarySwitches;
    m_nrInvoluntarySwitches = kv["nonvoluntary_ctxt_switches"].toInt();
    if (m_lastnrInvoluntarySwitches == 0) {
        m_lastnrInvoluntarySwitches = m_nrInvoluntarySwitches;
    }
    emit nrInvoluntarySwitchesChanged();

    return true;
}

const int updateInterval = 4000;

void ProcessInfo::calculatePowerImpact()
{
    double newImpact = 0;

    // penalize active switches heavily
    newImpact += double(m_nrInvoluntarySwitches - m_lastnrInvoluntarySwitches) / double(updateInterval);

    // penalize syscall switches too; but less heavily
    newImpact += (double(m_nrVoluntarySwitches - m_lastnrVoluntarySwitches) / double(updateInterval)) * 0.8;

    // anything playing audio is causing quite a bit of load even if it isn't in
    // itself taking power - so be sure to try attribute it correctly.
    newImpact += double(m_activePulseStreams * 100) / double(updateInterval);

    // TODO /proc/pid/io
    // should account for syscalls and bytes, syscalls heavier than bytes.

    if (m_powerImpact != newImpact) {
        m_powerImpact = newImpact;
        emit powerImpactChanged();
    }
}

ProcessModel::ProcessModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QTimer *t = new QTimer(this);
    t->setInterval(updateInterval);
    t->setSingleShot(false);
    t->start();
    connect(t, &QTimer::timeout, this, &ProcessModel::gatherInfo);
    gatherInfo();
}

ProcessInfo* ProcessModel::findProcess(ProcessId pid)
{
    bool found = false;
    for (const auto& it : m_processes) {
        if (it->pid() == pid) {
            return it;
        }
    }

    return nullptr;
}

void ProcessModel::updateProcesses()
{
    for (const auto& process : m_processes) {
        process->setLive(false);
    }

    QDirIterator procIt("/proc");
    while (procIt.hasNext()) {
        procIt.next();

        ProcessId pid = 0;
        bool ok = false;
        pid = procIt.fileName().toInt(&ok);
        if (!ok) {
            // non-pid
            continue;
        }

        ProcessInfo *process = findProcess(pid);
        if (process) {
            // just update it
            process->update();
        } else {
            // ### batch insert on startup
            ProcessInfo *process = new ProcessInfo(this, pid);
            connect(process, &ProcessInfo::processDead, this, [=]() {
                for (int i = 0; i < m_processes.size(); i++) {
                    if (m_processes.at(i) == process) {
                        beginRemoveRows(QModelIndex(), i, i);
                        m_processes.removeAt(i);
                        endRemoveRows();
                        delete process;
                        break;
                    }
                }
            });
            process->update();
            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            m_processes.append(process);
            endInsertRows();
        }
    }
}

void ProcessModel::updatePulseAudio()
{
    for (const auto& p : m_processes) {
        p->resetActivePulseStreams();
    }

    QProcess *p = new QProcess(this);
    p->start("pactl", QStringList() << "list", QIODevice::ReadOnly);
    p->waitForStarted();
    p->waitForFinished();

    QByteArray pulseOutput = p->readAllStandardOutput();
    bool processingInput = false;
    bool sourceCorked = false;
    qint64 sourcePid = 0;

    for (const QString& line : pulseOutput.split('\n')) {
        if (processingInput && line.trimmed() == "") {
            if (sourcePid != 0) {
                ProcessInfo *process = findProcess(sourcePid);
                if (process) {
                    if (!sourceCorked) {
                        process->incrementActivePulseStreams();
                    }
                } else {
                    qWarning() << "Couldn't find process" << sourcePid;
                }
            }
            processingInput = false;
            sourceCorked = false;
            sourcePid = 0;
            continue; // end of an element
        } else if (line.startsWith(' ') && !processingInput) {
            continue; // ignoring this line
        } else if (line.startsWith("Module ")) {
            continue; // don't care
        } else if (line.startsWith("Sink ")) {
            processingInput = true;
        } else if (line.startsWith("Source ")) {
            continue; // don't really care: we can't get much out of this?
        } else {
            if (processingInput) {
                if (line.contains("Corked: yes")) {
                    sourceCorked = true;
                } else if (line.contains("Corked: no")) {
                    sourceCorked = false;
                } else if (line.contains("application.process.id")) {
                    QStringList parts = line.split(" ");
                    if (parts.size() == 3) {
                        // application.process.id, =, "pid"
                        // it's quoted...
                        sourcePid = parts[2].mid(1, parts[2].size() - 2).toInt();
                    }
                }
            }
        }
    }
}

void ProcessModel::gatherInfo()
{
    updateProcesses();
    updatePulseAudio();

    if (rowCount() > 0) {
        emit dataChanged(index(0), index(rowCount() - 1));
    }
}

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    return m_processes.size();
}

QHash<int, QByteArray> ProcessModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(int(Role::ProcessInfoObject), "processInfoObject");
    roles.insert(int(Role::PID), "pid");
    roles.insert(int(Role::PPID), "ppid");
    roles.insert(int(Role::Name), "name");
    roles.insert(int(Role::NrVoluntarySwitches), "nrVoluntarySwitches");
    roles.insert(int(Role::NrInvoluntarySwitches), "nrInvoluntarySwitches");
    roles.insert(int(Role::ActivePulseStreams), "activePulseStreams");
    roles.insert(int(Role::PowerImpact), "powerImpact");
    roles.insert(int(Role::Live), "live");
    return roles;
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case int(Role::ProcessInfoObject):
        return QVariant::fromValue(m_processes.at(index.row()));
        break;
    case int(Role::PID):
        return m_processes.at(index.row())->pid();
        break;
    case int(Role::PPID):
        return m_processes.at(index.row())->ppid();
        break;
    case int(Role::Name):
        return m_processes.at(index.row())->name();
        break;
    case int(Role::NrVoluntarySwitches):
        return m_processes.at(index.row())->nrVoluntarySwitches();
        break;
    case int(Role::NrInvoluntarySwitches):
        return m_processes.at(index.row())->nrInvoluntarySwitches();
        break;
    case int(Role::ActivePulseStreams):
        return m_processes.at(index.row())->activePulseStreams();
        break;
    case int(Role::PowerImpact):
        return m_processes.at(index.row())->powerImpact();
        break;
    case int(Role::Live):
        return m_processes.at(index.row())->live();
        break;
    }
    return QVariant();
}
