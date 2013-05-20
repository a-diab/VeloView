#include "pqVelodyneManager.h"
#include "vvLoadDataReaction.h"
#include "vvCalibrationDialog.h"
#include "vvPythonQtDecorators.h"

#include <pqActiveObjects.h>
#include <pqActiveView.h>
#include <pqApplicationCore.h>
#include <pqDataRepresentation.h>
#include <pqPipelineSource.h>
#include <pqPythonDialog.h>
#include <pqPythonManager.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServer.h>
#include <pqServerManagerModel.h>
#include <pqSettings.h>
#include <pqView.h>

#include <vtkSMPropertyHelper.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTimerLog.h>

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>


//-----------------------------------------------------------------------------
class pqVelodyneManager::pqInternal
{
public:

  pqInternal()
  {

  }

  QAction* OpenFile;
  QAction* Close;
  QAction* OpenSensor;
  QAction* ChooseCalibrationFile;
  QAction* ResetView;
  QAction* Play;
  QAction* SeekForward;
  QAction* SeekBackward;
  QAction* GotoStart;
  QAction* GotoEnd;
  QAction* Record;
  QAction* MeasurementGrid;
  QAction* SaveCSV;
};

//-----------------------------------------------------------------------------
QPointer<pqVelodyneManager> pqVelodyneManagerInstance = NULL;

//-----------------------------------------------------------------------------
pqVelodyneManager *pqVelodyneManager::instance()
{
  if (!pqVelodyneManagerInstance)
    {
    pqVelodyneManagerInstance = new pqVelodyneManager(pqApplicationCore::instance());
    }

  return pqVelodyneManagerInstance;
}

//-----------------------------------------------------------------------------
pqVelodyneManager::pqVelodyneManager(QObject *p) : QObject(p)
{
  this->Internal = new pqInternal;
}

//-----------------------------------------------------------------------------
pqVelodyneManager::~pqVelodyneManager()
{
  delete this->Internal;
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::pythonStartup()
{

  QStringList pythonDirs;
  pythonDirs << "/source/velodyne/velodyneviewer/VelodyneHDL/python"
             << "c:/velodyne/build/velodyneviewer/src/velodyneviewer/VelodyneHDL/python"
             << QCoreApplication::applicationDirPath()  + "/../Python"
             << QCoreApplication::applicationDirPath()  + "/site-packages";

  QString pythonDir = QCoreApplication::applicationDirPath();
  foreach (const QString& dirname, pythonDirs)
    {
      if (QDir(dirname).exists())
        {
        pythonDir = dirname;
        break;
        }
    }


  this->runPython(QString(
      "import sys\n"
      "sys.path.insert(0, '%1')\n"
      "import PythonQt\n"
      "QtGui = PythonQt.QtGui\n"
      "QtCore = PythonQt.QtCore\n"
      "import veloview.applogic as vv\n"
      "vv.start()\n").arg(pythonDir));

  PythonQt::self()->addDecorators(new vvPythonQtDecorators());

  this->onMeasurementGrid();

  bool showDialogAtStartup = false;
  if (showDialogAtStartup)
    {
    pqPythonManager* manager = pqPVApplicationCore::instance()->pythonManager();
    pqPythonDialog* dialog = manager->pythonShellDialog();
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    }
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::runPython(const QString& statements)
{
  //printf("runPython(\"%s\")\n", qPrintable(statements));
  pqPythonManager* manager = pqPVApplicationCore::instance()->pythonManager();
  pqPythonDialog* dialog = manager->pythonShellDialog();
  dialog->runString(statements);
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::setup(QAction* openFile, QAction* close, QAction* openSensor,
  QAction* chooseCalibrationFile, QAction* resetView, QAction* play, QAction* seekForward, QAction* seekBackward,  QAction* gotoStart, QAction* gotoEnd,
  QAction* record, QAction* measurementGrid, QAction* saveScreenshot, QAction* saveCSV)
{
  this->Internal->OpenFile = openFile;
  this->Internal->Close = close;
  this->Internal->OpenSensor = openSensor;
  this->Internal->ChooseCalibrationFile = chooseCalibrationFile;
  this->Internal->ResetView = resetView;
  this->Internal->Play = play;
  this->Internal->SeekForward = seekForward;
  this->Internal->SeekBackward = seekBackward;
  this->Internal->GotoStart = gotoStart;
  this->Internal->GotoEnd = gotoEnd;
  this->Internal->Record = record;
  this->Internal->MeasurementGrid = measurementGrid;
  this->Internal->SaveCSV = saveCSV;

  pqSettings* settings = pqApplicationCore::instance()->settings();
  bool gridVisible = settings->value("VelodyneHDLPlugin/MeasurementGrid/Visibility", true).toBool();
  measurementGrid->setChecked(gridVisible);

  this->connect(openSensor, SIGNAL(triggered()), SLOT(onOpenSensor()));
  this->connect(chooseCalibrationFile, SIGNAL(triggered()), SLOT(onChooseCalibrationFile()));

  this->connect(measurementGrid, SIGNAL(triggered()), SLOT(onMeasurementGrid()));

  new vvLoadDataReaction(openFile);

  QTimer::singleShot(0, this, SLOT(pythonStartup()));
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::openData(const QString& filename)
{
  if (QFileInfo(filename).suffix() == "pcap")
    {
      vvCalibrationDialog dialog;
      int accepted = dialog.exec();

      if (!accepted)
        {
        return;
        }

      QString calibrationFile = dialog.selectedCalibrationFile();

      this->runPython(QString("vv.openPCAP('%1', '%2')\n").arg(filename).arg(calibrationFile));
    }
  else
    {
    this->runPython(QString("vv.openData('%1')\n").arg(filename));
    }
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::onMeasurementGrid()
{
  bool gridVisible = this->Internal->MeasurementGrid->isChecked();
  pqSettings* settings = pqApplicationCore::instance()->settings();
  settings->setValue("VelodyneHDLPlugin/MeasurementGrid/Visibility", gridVisible);

  if (gridVisible)
    {
    this->runPython("vv.showMeasurementGrid()\n");
    }
  else
    {
    this->runPython("vv.hideMeasurementGrid()\n");
    }
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::onOpenSensor()
{
  vvCalibrationDialog dialog;
  int accepted = dialog.exec();

  if (!accepted)
    {
    return;
    }

  QString calibrationFile = dialog.selectedCalibrationFile();

  this->runPython(QString("vv.openSensor('%1')\n").arg(calibrationFile));
}

//-----------------------------------------------------------------------------
void pqVelodyneManager::onChooseCalibrationFile()
{
  vvCalibrationDialog dialog;
  int accepted = dialog.exec();

  if (!accepted)
    {
    return;
    }

  QString calibrationFile = dialog.selectedCalibrationFile();

  this->runPython(QString("vv.setCalibrationFile('%1')\n").arg(calibrationFile));
}

//-----------------------------------------------------------------------------
pqServer *pqVelodyneManager::getActiveServer()
{
  pqApplicationCore *app = pqApplicationCore::instance();
  pqServerManagerModel *smModel = app->getServerManagerModel();
  pqServer *server = smModel->getItemAtIndex<pqServer*>(0);
  return server;
}

//-----------------------------------------------------------------------------
QWidget *pqVelodyneManager::getMainWindow()
{
  foreach(QWidget *topWidget, QApplication::topLevelWidgets())
    {
    if (qobject_cast<QMainWindow*>(topWidget))
      {
      return topWidget;
      }
    }
  return NULL;
}

//-----------------------------------------------------------------------------
pqView *pqVelodyneManager::findView(pqPipelineSource *source, int port, const QString &viewType)
{
  // Step 1, try to find a view in which the source is already shown.
  if (source)
    {
    foreach (pqView *view, source->getViews())
      {
      pqDataRepresentation *repr = source->getRepresentation(port, view);
      if (repr && repr->isVisible()) return view;
      }
    }

  // Step 2, check to see if the active view is the right type.
  pqView *view = pqActiveView::instance().current();
  if (view->getViewType() == viewType) return view;

  // Step 3, check all the views and see if one is the right type and not
  // showing anything.
  pqApplicationCore *core = pqApplicationCore::instance();
  pqServerManagerModel *smModel = core->getServerManagerModel();
  foreach (view, smModel->findItems<pqView*>())
    {
    if (   view && (view->getViewType() == viewType)
        && (view->getNumberOfVisibleRepresentations() < 1) )
      {
      return view;
      }
    }

  // Give up.  A new view needs to be created.
  return NULL;
}

//-----------------------------------------------------------------------------
pqView* pqVelodyneManager::getRenderView()
{
  return findView(0, 0, pqRenderView::renderViewType());
}