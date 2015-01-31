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

// Own includes
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pod.h"
#include "poddialog.h"

// Qt includes
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {
    setObjectName("MainWindow");
    setupStdOutRedirect();

    ui->setupUi(this);

    _localPods = new PodsModel();
    _remotePods = new PodsModel();

    _localPodsProxyModel = new QSortFilterProxyModel();
    _localPodsProxyModel->setSourceModel(_localPods);
    _localPodsProxyModel->setFilterKeyColumn(-1);

    _remotePodsProxyModel = new QSortFilterProxyModel();
    _remotePodsProxyModel->setSourceModel(_remotePods);
    _remotePodsProxyModel->setFilterKeyColumn(-1);

    ui->tableViewLocal->setModel(_localPodsProxyModel);
    ui->tableViewRemote->setModel(_remotePodsProxyModel);

    _systemTrayIcon.setIcon(QIcon(":/icons/icons/system-upgrade.svg"));
    _systemTrayIcon.setToolTip("Qt Pods");
    _systemTrayIcon.show();

    loadSettings();
    refreshAvailablePods();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::stdOutActivated(int fileDescriptor) {
    char readBuffer[1024];
    int numberOfBytesRead = ::read(fileDescriptor, readBuffer, sizeof(readBuffer) - 1);
    if(numberOfBytesRead > 0) {
        // Terminate buffer - just in case.
        readBuffer[numberOfBytesRead] = (char)0;

        ui->plainTextEditDiagnostic->appendPlainText(readBuffer);
    }
}

void MainWindow::setupStdOutRedirect() {
#ifdef Q_OS_UNIX
    // Redirect our own stdout/stderr.
    int pipeDescriptors[2];
    if(::pipe(pipeDescriptors) == 0) {
        ::dup2(pipeDescriptors[1], STDOUT_FILENO);
        ::dup2(pipeDescriptors[1], STDERR_FILENO);
        _stdOutSocketNotifier = new QSocketNotifier(pipeDescriptors[0], QSocketNotifier::Read, this);
        connect(_stdOutSocketNotifier, SIGNAL(activated(int)),
                this, SLOT(stdOutActivated(int)));
    }
#endif
}

void MainWindow::on_toolButtonRepository_clicked() {
    QString directory = QFileDialog::getExistingDirectory(this, tr("Add git repository"));
    if(!directory.isEmpty()) {
        if(!_podManager.isGitRepository(directory)) {
            QMessageBox::warning(this,
                tr("Invalid repository"),
                tr("The directory you supplied does not appear to be the root of a valid git repository."));
            return;
        }

        if(ui->comboBoxCurrentRepository->findText(directory) == -1) {
            ui->comboBoxCurrentRepository->addItem(directory);
        }

        ui->comboBoxCurrentRepository->setCurrentText(directory);

        saveSettings();

        refreshLocalPods();
    }
}

void MainWindow::on_pushButtonRemoveRepository_clicked() {
    int confirmationResult = QMessageBox::warning(this,
        tr("Confirm removal"),
        tr("Are you sure you want to remove the repository \"%1\"?")
            .arg(ui->comboBoxCurrentRepository->currentText()), QMessageBox::No, QMessageBox::Yes);
    if(confirmationResult == QMessageBox::Yes) {
        ui->comboBoxCurrentRepository->removeItem(ui->comboBoxCurrentRepository->currentIndex());
        saveSettings();
    }
}

void MainWindow::on_comboBoxCurrentRepository_currentTextChanged(QString text) {
    Q_UNUSED(text);
    refreshLocalPods();
}

void MainWindow::on_lineEditSearchLocal_textChanged(QString text) {
    _localPodsProxyModel->setFilterWildcard(text);
}

void MainWindow::on_lineEditSearchRemote_textChanged(QString text) {
    _remotePodsProxyModel->setFilterWildcard(text);
}

void MainWindow::on_pushButtonRemoveLocalPods_clicked() {
    QModelIndexList modelIndices = ui->tableViewLocal->selectionModel()->selectedRows(0);

    if(modelIndices.count() == 0) {
        return;
    }

    int confirmationResult;
    if(modelIndices.count() == 1) {
        confirmationResult = QMessageBox::warning(this,
            tr("Confirm removal"),
            tr("Are you sure you want to remove \"%1\" from the repository?")
                .arg(modelIndices.at(0).data().toString()), QMessageBox::No, QMessageBox::Yes);
    } else {
        confirmationResult = QMessageBox::warning(this,
            tr("Confirm removal"),
            tr("Are you sure you want to remove \"%1\" pods from the repository?")
                .arg(modelIndices.count()), QMessageBox::No, QMessageBox::Yes);
    }

    if(confirmationResult == QMessageBox::Yes) {
        foreach(QModelIndex modelIndex, modelIndices) {
            _podManager.removePod(ui->comboBoxCurrentRepository->currentText(),
                                  _localPods->item(modelIndex.row(), 0)->text());
        }
        refreshLocalPods();
    }
}

void MainWindow::on_pushButtonUpdateLocalPods_clicked() {
    _podManager.updatePods(ui->comboBoxCurrentRepository->currentText());
    refreshLocalPods();
}

void MainWindow::on_pushButtonRefreshLocalPods_clicked() {
    ui->tableViewLocal->setEnabled(false);
    ui->pushButtonRemoveLocalPods->setEnabled(false);
    ui->pushButtonRefreshLocalPods->setEnabled(false);

    refreshLocalPods();

    ui->tableViewLocal->setEnabled(true);
    ui->pushButtonRemoveLocalPods->setEnabled(true);
    ui->pushButtonRefreshLocalPods->setEnabled(true);
}

void MainWindow::on_pushButtonRefreshAvailablePods_clicked() {
    ui->tableViewRemote->setEnabled(false);
    ui->pushButtonInstallPods->setEnabled(false);
    ui->pushButtonRefreshAvailablePods->setEnabled(false);

    refreshAvailablePods();

    ui->tableViewRemote->setEnabled(true);
    ui->pushButtonInstallPods->setEnabled(true);
    ui->pushButtonRefreshAvailablePods->setEnabled(true);
}

void MainWindow::on_pushButtonInstallPods_clicked() {
    QModelIndexList modelIndices = ui->tableViewRemote->selectionModel()->selectedRows(0);
    QList<Pod> pods;
    foreach(QModelIndex modelIndex, modelIndices) {
        Pod pod;
        pod.name = _remotePods->item(modelIndex.row(), 0)->text();
        pod.url = _remotePods->item(modelIndex.row(), 1)->text();
        pods.append(pod);
    }

    foreach(Pod pod, pods) {
        _podManager.installPod(ui->comboBoxCurrentRepository->currentText(), pod);
    }

    refreshLocalPods();
    ui->tabWidgetPods->setCurrentIndex(0);
}

void MainWindow::on_pushButtonInstallExternalPod_clicked() {
    PodDialog podDialog;
    if(podDialog.exec() == QDialog::Accepted) {
        Pod pod = podDialog.pod();
        if(_podManager.installPod(ui->comboBoxCurrentRepository->currentText(), pod)) {
            QMessageBox::information(this, tr("Pod has been installed"), tr("The pod has been installed successfully."));
            refreshLocalPods();
            ui->tabWidgetPods->setCurrentIndex(0);
        } else {
            QMessageBox::critical(this, tr("Error install pod"), tr("The pod could not be installed."));
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *closeEvent) {
    saveSettings();
    QMainWindow::closeEvent(closeEvent);
}

void MainWindow::loadSettings() {
    QSettings settings("qt-pods", "qt-pods");

    QStringList repositories = settings.value("local-repositories").toStringList();
    ui->comboBoxCurrentRepository->addItems(repositories);
}

void MainWindow::saveSettings() {
    QSettings settings("qt-pods", "qt-pods");

    QStringList repositories;
    for(int i = 0; i < ui->comboBoxCurrentRepository->count(); i++) {
        repositories.append(ui->comboBoxCurrentRepository->itemText(i));
    }
    settings.setValue("local-repositories", repositories);
    settings.sync();
}

void MainWindow::refreshLocalPods() {
    QString repository = ui->comboBoxCurrentRepository->currentText();
    if(!_podManager.isGitRepository(repository)) {
        QMessageBox::warning(this,
            tr("Invalid repository"),
            tr("The directory you supplied does not appear to be the root of a valid git repository."));

        int index;
        while((index = ui->comboBoxCurrentRepository->findText(repository)) != -1) {
            ui->comboBoxCurrentRepository->removeItem(index);
        }
        return;
    }

    QList<Pod> pods = _podManager.installedPods(repository);

    // Clear model
    _localPods->reset();
    foreach(Pod pod, pods) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem(pod.name));
        row.append(new QStandardItem(pod.url));

        if(!_podManager.checkPod(repository, pod.name)) {
            QFont font = row.at(0)->font();
            font.setItalic(true);
            row.at(0)->setFont(font);
            row.at(1)->setFont(font);
        }

        foreach(QStandardItem *item, row) {
            item->setEditable(false);
        }

        _localPods->appendRow(row);
    }
}

void MainWindow::refreshAvailablePods() {
    QStringList sources;
    sources << "https://raw.githubusercontent.com/cybercatalyst/qt-pods-master/master/pods.json";

    QList<Pod> pods = _podManager.availablePods(sources);

    // Clear model
    _remotePods->reset();
    foreach(Pod pod, pods) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem(pod.name));
        row.append(new QStandardItem(pod.url));

        foreach(QStandardItem *item, row) {
            item->setEditable(false);
        }

        _remotePods->appendRow(row);
    }
}
