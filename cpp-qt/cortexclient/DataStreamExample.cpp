/***************
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
***************/
#include "DataStreamExample.h"
#include "Config.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QtDebug>

#include <queue>

using namespace std;

static queue <QJsonArray> receive_fifo;
int rcv_timeout = 3;

DataStreamExample::DataStreamExample(QObject *parent) : QObject(parent) {
    connect(&client, &CortexClient::connected, this, &DataStreamExample::onConnected);
    connect(&client, &CortexClient::disconnected, this, &DataStreamExample::onDisconnected);
    connect(&client, &CortexClient::errorReceived, this, &DataStreamExample::onErrorReceived);
    connect(&client, &CortexClient::loadProfileOk, this, &DataStreamExample::onLoadProfileOk);
    connect(&client, &CortexClient::subscribeOk, this, &DataStreamExample::onSubscribeOk);
    connect(&client, &CortexClient::unsubscribeOk, this, &DataStreamExample::onUnsubscribeOk);
    connect(&client, &CortexClient::streamDataReceived, this, &DataStreamExample::onStreamDataReceived);
    connect(&client, &CortexClient::closeSessionOk, this, &DataStreamExample::onCloseSessionOk);
    connect(&finder, &HeadsetFinder::headsetFound, this, &DataStreamExample::onHeadsetFound);
    connect(&creator, &SessionCreator::sessionCreated, this, &DataStreamExample::onSessionCreated);
}

void DataStreamExample::start(QString stream, bool activateSession, QString license) {
    this->stream = stream;
    this->activateSession = activateSession;
    this->license = license;
    nextDataTime = 0;
    client.open();
}

void DataStreamExample::set_receive_timeout(int t)
{
    if(t == 0)
    {
        qInfo() << "Timeout is set to zero, streaming won't stop unless user interrupts.";
    }

    rcv_timeout = t;
}


bool DataStreamExample::readFIFO(QJsonArray *data)
{
    if(!receive_fifo.empty())
    {
        QJsonArray temp = receive_fifo.front();

        memcpy(data, &temp, sizeof(temp));

        receive_fifo.pop();
        return true;
    }
    else return false;
}


void DataStreamExample::onConnected() {
    qInfo() << "Connected to Cortex.";
    finder.findHeadsets(&client);
}

void DataStreamExample::onDisconnected() {
    qInfo() << "Disconnected.";
    QCoreApplication::quit();
}

void DataStreamExample::onErrorReceived(QString method) {
    if (method == "setupProfile") {
        // it's fine, we can subscribe to a data stream even without a profile
        qInfo() << "Failed to load the training profile.";
    }
    else {
        QCoreApplication::quit();
    }
}

void DataStreamExample::onHeadsetFound(const Headset &headset) {
    finder.clear();
    this->headset = headset;
    // next step: create a session for this headset
    creator.createSession(&client, headset, activateSession, license);
}

void DataStreamExample::onSessionCreated(QString token, QString sessionId) {
    creator.clear();
    this->token = token;
    this->sessionId = sessionId;
    // load the training profile (useful only for mental command and facial expression)
    client.loadProfile(token, headset.id, TrainingProfileName);
    // next step: subscribe to a data stream
    client.subscribe(token, sessionId, stream);
}

void DataStreamExample::onLoadProfileOk(QString profileName)
{
    qInfo() << "Training profile loaded" << profileName;
}

void DataStreamExample::onSubscribeOk(QStringList streams) {
    qInfo() << "Subscription successful for data streams" << streams;
    qInfo() << "Receiving data for"  << rcv_timeout <<  "seconds.";

    if(rcv_timeout)
    {
        QTimer::singleShot(rcv_timeout*1000, this, &DataStreamExample::unsubscribe);
    }

}

void DataStreamExample::onStreamDataReceived(
        QString sessionId, QString stream, double time, const QJsonArray &data) {
    Q_UNUSED(sessionId);

        while(receive_fifo.size() >= MAX_FIFO_SIZE)
    {
        receive_fifo.pop();
    }

    receive_fifo.push(data);
    // a data stream can publish data with a high frequency
    // we display only a few samples per second
    if (time >= nextDataTime) {
        qInfo() << stream << data;
        nextDataTime = time + 0.2;
    }
}

void DataStreamExample::stop() {
    client.unsubscribe(token, sessionId, stream);
}


void DataStreamExample::unsubscribe() {
    client.unsubscribe(token, sessionId, stream);
}

void DataStreamExample::onUnsubscribeOk(QStringList streams) {
    qInfo() << "Subscription cancelled for data streams" << streams;
    client.closeSession(token, sessionId);
}

void DataStreamExample::onCloseSessionOk() {
    qInfo() << "Session closed.";
    client.close();
}
