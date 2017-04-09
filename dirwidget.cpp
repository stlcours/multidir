#include "dirwidget.h"
#include "proxymodel.h"
#include "filesystemmodel.h"

#include <QTableView>
#include <QBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QToolButton>
#include <QHeaderView>
#include <QSettings>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QMessageBox>

#include <QDebug>

namespace
{
const QString qs_dir = "dir";
const QString qs_isLocked = "locked";
const QString qs_view = "view";
const QString qs_showDirs = "showDirs";
}

DirWidget::DirWidget (FileSystemModel *model, QWidget *parent) :
  QWidget (parent),
  model_ (model),
  proxy_ (new ProxyModel (model, this)),
  view_ (new QTableView (this)),
  menu_ (new QMenu (this)),
  openAction_ (nullptr),
  renameAction_ (nullptr),
  removeAction_ (nullptr),
  pathLabel_ (new QLabel (this)),
  dirLabel_ (new QLabel (this)),
  isLocked_ (nullptr),
  up_ (new QToolButton (this)),
  showDirs_ (new QToolButton (this)),
  controlsLayout_ (new QHBoxLayout)
{
  setContextMenuPolicy (Qt::CustomContextMenu);
  connect (this, &QWidget::customContextMenuRequested,
           this, &DirWidget::showContextMenu);

  proxy_->setDynamicSortFilter (true);

  view_->setModel (proxy_);
  view_->setSortingEnabled (true);
  view_->setSelectionBehavior (QAbstractItemView::SelectRows);
  view_->setEditTriggers (QAbstractItemView::SelectedClicked);
  view_->setDragDropMode (QAbstractItemView::DragDrop);
  view_->setDragDropOverwriteMode (false);
  view_->setDefaultDropAction (Qt::MoveAction);
  connect (view_, &QTableView::doubleClicked,
           this, &DirWidget::openPath);

  view_->horizontalHeader ()->setContextMenuPolicy (Qt::CustomContextMenu);
  connect (view_->horizontalHeader (), &QWidget::customContextMenuRequested,
           this, &DirWidget::showHeaderContextMenu);


  openAction_ = menu_->addAction (tr ("Open"));
  connect (openAction_, &QAction::triggered,
           this, [this]() {openPath (view_->currentIndex ());});

  renameAction_ = menu_->addAction (QIcon (":/rename.png"), tr ("Rename"));
  connect (renameAction_, &QAction::triggered,
           this, &DirWidget::startRenaming);

  removeAction_ = menu_->addAction (QIcon (":/remove.png"), tr ("Remove..."));
  connect (removeAction_, &QAction::triggered,
           this, &DirWidget::promptRemove);

  menu_->addSeparator ();

  isLocked_ = menu_->addAction (QIcon (":/lockTab.png"), tr ("Lock tab"));
  isLocked_->setCheckable (true);
  connect (isLocked_, &QAction::toggled,
           this, &DirWidget::setIsLocked);

  auto clone = menu_->addAction (QIcon (":/cloneTab.png"), tr ("Clone tab"));
  connect (clone, &QAction::triggered,
           this, [this]() {emit cloneRequested (this);});

  menu_->addSeparator ();

  auto close = menu_->addAction (QIcon (":/closeTab.png"), tr ("Close tab..."));
  connect (close, &QAction::triggered,
           this, &DirWidget::promptClose);

  connect (menu_, &QMenu::aboutToShow,
           this, &DirWidget::updateMenu);


  up_->setIcon (QIcon (":/up.png"));
  up_->setToolTip (tr ("Move up"));
  connect (up_, &QToolButton::pressed,
           this, &DirWidget::moveUp);

  showDirs_->setIcon (QIcon (":/folder.png"));
  showDirs_->setToolTip (tr ("Show directories"));
  showDirs_->setCheckable (true);
  showDirs_->setChecked (proxy_->showDirs ());
  connect (showDirs_, &QToolButton::toggled,
           this, &DirWidget::toggleShowDirs);


  pathLabel_->setAlignment (pathLabel_->alignment () | Qt::AlignRight);
  auto dirFont = dirLabel_->font ();
  dirFont.setBold (true);
  dirLabel_->setFont (dirFont);

  controlsLayout_->addStretch (1);
  controlsLayout_->addWidget (pathLabel_);
  controlsLayout_->addWidget (dirLabel_);
  controlsLayout_->addStretch (1);
  controlsLayout_->addWidget (up_);
  controlsLayout_->addWidget (showDirs_);

  auto layout = new QVBoxLayout (this);
  layout->addLayout (controlsLayout_);
  layout->addWidget (view_);

  setIsLocked (false);
}

DirWidget::~DirWidget ()
{

}

void DirWidget::save (QSettings &settings) const
{
  settings.setValue (qs_dir, path (view_->rootIndex ()));
  settings.setValue (qs_isLocked, isLocked ());
  settings.setValue (qs_view, view_->horizontalHeader ()->saveState ());
  settings.setValue (qs_showDirs, proxy_->showDirs ());
}

void DirWidget::restore (QSettings &settings)
{
  setPath (settings.value (qs_dir).toString ());
  isLocked_->setChecked (settings.value (qs_isLocked, isLocked ()).toBool ());
  if (settings.contains (qs_view))
  {
    view_->horizontalHeader ()->restoreState (settings.value (qs_view).toByteArray ());
  }
  showDirs_->setChecked (settings.value (qs_showDirs, true).toBool ());
}

void DirWidget::setPath (const QString &path)
{
  auto absolutePath = QDir (path).absolutePath ();
  auto index = proxy_->mapFromSource (model_->index (absolutePath));
  if (!index.isValid ())
  {
    return;
  }
  view_->setRootIndex (index);
  proxy_->setCurrent (index);

  pathLabel_->setText (fittedPath ());
  auto nameIndex = absolutePath.lastIndexOf (QLatin1Char ('/')) + 1;
  dirLabel_->setText (nameIndex ? absolutePath.mid (nameIndex) : absolutePath);
}

QString DirWidget::path () const
{
  return path (view_->rootIndex ());
}

void DirWidget::setNameFilter (const QString &filter)
{
  proxy_->setNameFilter (QLatin1String ("*") + filter + QLatin1String ("*"));
}

void DirWidget::setIsLocked (bool isLocked)
{
  up_->setEnabled (!isLocked);
}

void DirWidget::openPath (const QModelIndex &index)
{
  if (!index.isValid ())
  {
    return;
  }

  const auto mapped = proxy_->mapToSource (index);
  auto path = model_->filePath (mapped);
  if (!model_->isDir (mapped))
  {
    QDesktopServices::openUrl (QUrl::fromLocalFile (path));
  }
  else
  {
    if (!isLocked () && model_->permissions (mapped) & QFile::ExeUser)
    {
      setPath (path);
    }
  }
}

QString DirWidget::path (const QModelIndex &index) const
{
  return model_->filePath (proxy_->mapToSource (index));
}

bool DirWidget::isLocked () const
{
  return isLocked_->isChecked ();
}

void DirWidget::moveUp ()
{
  QDir dir (path (view_->rootIndex ()));
  if (dir.cdUp ())
  {
    setPath (dir.absolutePath ());
  }
}

void DirWidget::toggleShowDirs (bool show)
{
  proxy_->setShowDirs (show);
}

void DirWidget::showContextMenu ()
{
  menu_->exec (QCursor::pos ());
}

void DirWidget::showHeaderContextMenu ()
{
  QMenu menu;
  auto header = view_->horizontalHeader ();
  for (auto i = 0, end = proxy_->columnCount (); i < end; ++i)
  {
    auto action = menu.addAction (proxy_->headerData (i, Qt::Horizontal).toString ());
    action->setCheckable (true);
    action->setChecked (!header->isSectionHidden (i));
    action->setData (i);
  }

  auto choice = menu.exec (QCursor::pos ());
  if (choice)
  {
    header->setSectionHidden (choice->data ().toInt (), !choice->isChecked ());
  }

}

void DirWidget::resizeEvent (QResizeEvent */*event*/)
{
  const auto newText = fittedPath ();
  if (newText != pathLabel_->text ())
  {
    pathLabel_->setText (newText);
  }
}

QString DirWidget::fittedPath () const
{
  auto path = this->path ();
  const auto nameIndex = path.lastIndexOf (QLatin1Char ('/')) + 1;
  if (!nameIndex)
  {
    return {};
  }

  const auto stretchWidth = controlsLayout_->itemAt (0)->geometry ().width ();
  const auto maxWidth = pathLabel_->width () + 2 * stretchWidth - 10; // 10 just for sure
  const QString prepend = QLatin1String (".../");
  const auto searchStartIndex = prepend.length ();

  QFontMetrics metrics (pathLabel_->font ());
  path = path.left (nameIndex);
  auto width = metrics.boundingRect (path).width ();

  while (width > maxWidth)
  {
    auto index = path.indexOf (QLatin1Char ('/'), searchStartIndex);
    if (index == -1)
    {
      break;
    }
    path = prepend + path.mid (index + 1);
    width = metrics.boundingRect (path).width ();
  }
  return path;
}

void DirWidget::updateMenu ()
{
  const auto index = view_->currentIndex ();
  const auto isDotDot = index.isValid () && index.data () == QLatin1String ("..");
  openAction_->setEnabled (index.isValid ());
  renameAction_->setEnabled (index.isValid () && !isDotDot);
  removeAction_->setEnabled (index.isValid () && !isDotDot);
}

void DirWidget::startRenaming ()
{
  const auto index = view_->currentIndex ();
  const auto nameIndex = index.sibling (index.row (), FileSystemModel::Column::Name);
  view_->setCurrentIndex (nameIndex);
  view_->edit (nameIndex);
}

void DirWidget::promptClose ()
{
  auto res = QMessageBox::question (this, {}, tr ("Close tab \"%1\"?").arg (path ()),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (res == QMessageBox::Yes)
  {
    emit closeRequested (this);
  }
}

void DirWidget::promptRemove ()
{
  const auto indexes = view_->selectionModel ()->selectedRows (FileSystemModel::Column::Name);
  if (indexes.isEmpty ())
  {
    return;
  }
  QStringList names;
  for (const auto &i: indexes)
  {
    names << i.data ().toString ();
  }
  auto res = QMessageBox::question (this, {}, tr ("Remove \"%1\" permanently?")
                                    .arg (names.join (QLatin1String ("\", \""))),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (res == QMessageBox::Yes)
  {
    for (const auto &i: indexes)
    {
      model_->remove (proxy_->mapToSource (i));
    }
  }
}
