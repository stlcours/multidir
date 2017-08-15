#pragma once

#include <QSystemTrayIcon>
#include <QWidget>

class GroupsMenu;
class GroupsView;
class FileSystemModel;
class FileOperation;
class FileConflictResolver;

class QSettings;
class QAction;
class QLineEdit;

class MainWindow : public QWidget
{
Q_OBJECT
public:
  explicit MainWindow (QWidget *parent = nullptr);
  ~MainWindow ();

  void save (QSettings &settings) const;
  void restore (QSettings &settings);

public slots:
  void updateSettings ();

protected:
  void keyPressEvent (QKeyEvent *event) override;

private:
  void toggleVisible ();
  void editSettings ();
  void showAbout ();
  void activateFindMode ();

  void updateTrayMenu ();
  void handleTrayClick (QSystemTrayIcon::ActivationReason reason);
  void setCheckUpdates (bool isOn);
  void showFileOperation (QSharedPointer<FileOperation> operation);
  void updateWindowTitle (const QString &groupName);

  FileSystemModel *model_;
  GroupsView *groups_;
  GroupsMenu *groupsMenu_;
  FileConflictResolver *conflictResolver_;
  QLineEdit *findEdit_;
  QLayout *fileOperationsLayout_;
  QSystemTrayIcon *tray_;
  QAction *toggleAction_;
  bool checkUpdates_;
  bool startInBackground_;
};
