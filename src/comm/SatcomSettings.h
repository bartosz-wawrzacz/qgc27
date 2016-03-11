#ifndef SATCOMSETTINGS_H
#define SATCOMSETTINGS_H

#include <QSettings>
#include "QGCMAVLink.h"
#include "SatcomSettings.h"

class SatcomSettings : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(bool             satcomUsed          READ satcomUsed         WRITE setSatcomUsed         NOTIFY satcomUsedChanged)
    Q_PROPERTY(int              vehicleId           READ vehicleId          WRITE setVehicleId          NOTIFY vehicleIdChanged)
    Q_PROPERTY(MAV_AUTOPILOT    vehicleFirmwareVer  READ vehicleFirmwareVer WRITE setVehicleFirmwareVer NOTIFY vehicleFirmwareVerChanged)
    Q_PROPERTY(MAV_TYPE         vehicleType         READ vehicleType        WRITE setVehicleType        NOTIFY vehicleTypeChanged)

    bool            satcomUsed()            { return _satcomUsed; }
    int             vehicleId()             { return _vehicleId; }
    MAV_AUTOPILOT   vehicleFirmwareVer()    { return _vehicleFirmwareVer; }
    MAV_TYPE        vehicleType()           { return _vehicleType; }

    SatcomSettings();
    SatcomSettings(SatcomSettings* copy);

    void setSatcomUsed(bool used)                   { _satcomUsed = used; emit satcomUsedChanged(); }
    void setVehicleId(int id)                       { _vehicleId = id; emit vehicleIdChanged(); }
    void setVehicleFirmwareVer(MAV_AUTOPILOT ver)   { _vehicleFirmwareVer = ver; emit vehicleFirmwareVerChanged(); }
    void setVehicleType(MAV_TYPE type)              { _vehicleType = type; emit vehicleTypeChanged(); }

    void loadSettings(QSettings& settings, const QString& root);
    void saveSettings(QSettings& settings, const QString& root);

signals:
    void satcomUsedChanged();
    void vehicleIdChanged();
    void vehicleFirmwareVerChanged();
    void vehicleTypeChanged();

private:
    bool            _satcomUsed;
    int             _vehicleId;
    MAV_AUTOPILOT   _vehicleFirmwareVer;
    MAV_TYPE        _vehicleType;
};

#endif // SATCOMSETTINGS_H
