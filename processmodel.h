#ifndef PROCESSMODEL_H
#define PROCESSMODEL_H

#include <QAbstractListModel>
#include <QDebug>

typedef uint ProcessId;

class ProcessInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProcessId pid READ pid CONSTANT)
    Q_PROPERTY(ProcessId ppid READ ppid CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(qint64 nrVoluntarySwitches READ nrVoluntarySwitches NOTIFY nrVoluntarySwitchesChanged)
    Q_PROPERTY(qint64 nrInvoluntarySwitches READ nrInvoluntarySwitches NOTIFY nrInvoluntarySwitchesChanged)
    Q_PROPERTY(qint64 activePulseStreams READ activePulseStreams NOTIFY activePulseStreamsChanged)
    Q_PROPERTY(double powerImpact READ powerImpact NOTIFY powerImpactChanged)
    Q_PROPERTY(bool live READ live NOTIFY liveChanged)

public:
    ProcessInfo() {}
    ProcessInfo(QObject *parent, ProcessId pid) : QObject(parent), m_pid(pid) {}

    bool gatherStatus();
    void calculatePowerImpact();
    void update();

    ProcessId pid() const { return m_pid; }
    ProcessId ppid() const { return m_ppid; }
    QString name() const { return m_name; }
    qint64 nrVoluntarySwitches() const { return m_nrVoluntarySwitches; }
    qint64 nrInvoluntarySwitches() const { return m_nrInvoluntarySwitches; }
    qint64 activePulseStreams() const { return m_activePulseStreams; }
    double powerImpact() const { return m_powerImpact; }
    bool live() const { return m_live; }

    void resetActivePulseStreams() { m_activePulseStreams = 0; emit activePulseStreamsChanged(); }
    void incrementActivePulseStreams() { m_activePulseStreams++; emit activePulseStreamsChanged(); }
    void setLive(bool l);

signals:
    void nameChanged();
    void nrVoluntarySwitchesChanged();
    void nrInvoluntarySwitchesChanged();
    void activePulseStreamsChanged();
    void powerImpactChanged();
    void liveChanged();
    void processDead();

private:
    // /proc
    ProcessId m_pid = 0;

    // /proc/pid/status
    QString m_name;
    ProcessId m_ppid = 0;
    qint64 m_nrVoluntarySwitches = 0;
    qint64 m_lastnrVoluntarySwitches = 0;
    qint64 m_nrInvoluntarySwitches = 0;
    qint64 m_lastnrInvoluntarySwitches = 0;

    // pulseaudio
    qint64 m_activePulseStreams = 0;

    // calculated
    double m_powerImpact = 0;
    bool m_live = false;
    bool m_reallyDead = false;
};


class ProcessModel : public QAbstractListModel
{
    Q_OBJECT
public:
    ProcessModel(QObject *parent = nullptr);
    void gatherInfo();

protected:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    enum class Role {
        ProcessInfoObject,
        PID,
        PPID,
        Name,
        NrVoluntarySwitches,
        NrInvoluntarySwitches,
        ActivePulseStreams,
        PowerImpact, 
        Live,
    };

private:
    void updateProcesses();
    void updatePulseAudio();

    ProcessInfo* findProcess(ProcessId pid);

    QVector<ProcessInfo*> m_processes;
};

#endif // PROCESSMODEL_H
