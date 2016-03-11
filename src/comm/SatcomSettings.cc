#include "SatcomSettings.h"

SatcomSettings::SatcomSettings() :
    _satcomUsed(false),
    _vehicleId(0),
    _vehicleFirmwareVer(MAV_AUTOPILOT_GENERIC),
    _vehicleType(MAV_TYPE_GENERIC)
{}

SatcomSettings::SatcomSettings(SatcomSettings* copy)
{
    _satcomUsed = copy->satcomUsed();
    _vehicleId = copy->vehicleId();
    _vehicleFirmwareVer = copy->vehicleFirmwareVer();
    _vehicleType = copy->vehicleType();
}

void SatcomSettings::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    if(settings.contains("satcomUsed"))         _satcomUsed = settings.value("satcomUsed").toBool();
    if(settings.contains("vehicleId"))          _vehicleId = settings.value("vehicleId").toInt();
    if(settings.contains("vehicleFirmwareVer")) _vehicleFirmwareVer = (MAV_AUTOPILOT) settings.value("vehicleFirmwareVer").toInt();
    if(settings.contains("vehicleType"))        _vehicleType = (MAV_TYPE) settings.value("vehicleType").toInt();
    settings.endGroup();
}

void SatcomSettings::saveSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    settings.setValue("satcomUsed",         _satcomUsed);
    settings.setValue("vehicleId",          _vehicleId);
    settings.setValue("vehicleFirmwareVer", _vehicleFirmwareVer);
    settings.setValue("vehicleType",        _vehicleType);
    settings.endGroup();
}
