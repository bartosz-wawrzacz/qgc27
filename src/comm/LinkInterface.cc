#include <QApplication>

#include "LinkInterface.h"

#include "QGCApplication.h"

LinkInterface::LinkInterface() : QThread(0)
    , _mavlinkChannelSet(false)
    , _active(false)
    , _enableRateCollection(false)
{
    // Initialize everything for the data rate calculation buffers.
    _inDataIndex  = 0;
    _outDataIndex = 0;

    // Initialize our data rate buffers.
    memset(_inDataWriteAmounts, 0, sizeof(_inDataWriteAmounts));
    memset(_inDataWriteTimes,   0, sizeof(_inDataWriteTimes));
    memset(_outDataWriteAmounts,0, sizeof(_outDataWriteAmounts));
    memset(_outDataWriteTimes,  0, sizeof(_outDataWriteTimes));

    qRegisterMetaType<LinkInterface*>("LinkInterface*");

    _linkActiveTimer.setInterval(_linkActiveTimeoutMSecs);
    _linkActiveTimer.setSingleShot(true);
    QObject::connect(&_linkActiveTimer, &QTimer::timeout, this, &LinkInterface::_linkActiveTimeout);
}

void LinkInterface::setActive(bool active)
{
    if(active) {
        //qDebug() << ">>>>>>>>>>>>>>" << getName() << "starting timer";
        QObject::connect(&_linkActiveTimer, &QTimer::timeout, this, &LinkInterface::_linkActiveTimeout);
        _linkActiveTimer.start();
        //qDebug() << "active" << _linkActiveTimer.isActive() << "time" << _linkActiveTimer.remainingTime();
    }

    if(_active != active) {
        _active = active;
        emit activeChanged(active);
        //qDebug() << ">>>>>>>>>>>>>>>>>>>>> link" << getName() << "active:" << active;
    }
}

quint64 LinkInterface::timeSinceRxMs(void)
{
    return QGC::groundTimeMilliseconds() - _lastRxTime;
}

//void LinkInterface::updateLastRxTime(void)
//{
//    _lastRxTime = QGC::groundTimeMilliseconds();
//}

void LinkInterface::_linkActiveTimeout(void)
{
    qDebug() << "<<<<<<<<<<<<<<<<<< TIMEOUT DZIWKO" << getName();
    setActive(false);
}

void LinkInterface::receivedHeartbeat(void)
{
    qDebug() << getName() << "got HB";
    setActive(true);
    _lastRxTime = QGC::groundTimeMilliseconds();
}

//void LinkInterface::_linkInactive(void)
//{
//    setActive(false);
//}
