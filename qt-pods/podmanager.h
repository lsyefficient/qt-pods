///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//    This file is part of qt-pods.                                          //
//    Copyright (C) 2015 Jacob Dawid, jacob@omg-it.works                     //
//                                                                           //
//    qt-pods is free software: you can redistribute it and/or modify        //
//    it under the terms of the GNU General Public License as published by   //
//    the Free Software Foundation, either version 3 of the License, or      //
//    (at your option) any later version.                                    //
//                                                                           //
//    qt-pods is distributed in the hope that it will be useful,             //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of         //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          //
//    GNU General Public License for more details.                           //
//                                                                           //
//    You should have received a copy of the GNU General Public License      //
//    along with qt-pods. If not, see <http://www.gnu.org/licenses/>.        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef PODMANAGER_H
#define PODMANAGER_H

#include <QString>
#include <QObject>

#include <QNetworkAccessManager>

class PodManager : public QObject {
    Q_OBJECT
public:
    struct Pod {
        QString name;
        QString url;
    };

    PodManager(QObject *parent = 0);

    bool isValidRepository(QString repository);

    void installPod(QString repository, Pod pod);
    void removePod(QString repository, QString podName);
    void updatePods(QString repository);

    QList<Pod> installedPods(QString repository);
    QList<Pod> availablePods(QStringList sources);

    void generatePodsPri(QString repository);

    /**
     * Checks whether an installed pods is a valid pod, ie.
     * - pod name is all lowercase
     * - pod exists
     * - pod contains pro and pri
     * - pod contains LICENSE and README.md
     * @param repository
     * @param podName
     * @return
     */
    bool checkPod(QString repository, QString podName);

private:
    QNetworkAccessManager _networkAccessManager;
};

#endif // PODMANAGER_H