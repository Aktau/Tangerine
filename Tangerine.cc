#include "Tangerine.h"

#include <QtGui>
#include <QDebug>

#include <assert.h>

#include "SQLFragmentConf.h"
#include "Database.h"

using namespace thera;

const QString Tangerine::MATCH_COUNT_TEXT = "%1 total matches loaded";

const int Tangerine::MIN_WIDTH = 1280;
const int Tangerine::MIN_HEIGHT = 600;

Tangerine::Tangerine(SQLDatabase *db, const QDir& thumbDir, QWidget *parent) : QMainWindow(parent), mDb(*db), mModel(db), mSelectionModel(NULL), mThumbDir(thumbDir), mProgress(NULL), mNumberOfMatchesLabel(NULL) {
	mSelectionModel = new MatchSelectionModel(&mModel, this);

	setupWindow();

	// the ordering is important, the slots use instances made in setupWindow() et cetera

	connect(&mDb, SIGNAL(databaseOpened()), this, SLOT(databaseOpened()));
	connect(&mDb, SIGNAL(databaseClosed()), this, SLOT(databaseClosed()));
	connect(&mDb, SIGNAL(databaseOpStarted(const QString&, int)), this, SLOT(databaseOpStarted(const QString&, int)));
	connect(&mDb, SIGNAL(databaseOpStepDone(int)), this, SLOT(databaseOpStepDone(int)));
	connect(&mDb, SIGNAL(databaseOpEnded()), this, SLOT(databaseOpEnded()));
	connect(&mDb, SIGNAL(matchCountChanged()), this, SLOT(matchCountChanged()));

	databaseClosed();

	mLoadFragDbAct->setEnabled(!Database::isValid());
	mLoadMatchDbAct->setEnabled(Database::isValid());
	mImportXMLAct->setEnabled(Database::isValid());
}

Tangerine::~Tangerine() {
	closeDatabase();

	qDebug() << "Tangerine::~Tangerine: ran";
}

void Tangerine::setupWindow() {
	/* window configuration */

	setWindowIcon(QIcon("tangerine.ico"));
	setWindowTitle(QString("Tangerine %1 %2.%3").arg(DEV_PHASE).arg(MAJ_VERSION).arg(MIN_VERSION));
	setToolButtonStyle(Qt::ToolButtonIconOnly);

	/* create actions */

	createActions();

	/* menu bar */

	mFileMenu = menuBar()->addMenu(tr("&File"));
	mFileMenu->addAction(mLoadFragDbAct);
	mFileMenu->addAction(mLoadMatchDbAct);
	mFileMenu->addAction(mSaveDbAct);
	mFileMenu->addSeparator();
	mFileMenu->addAction(mImportXMLAct);
	mFileMenu->addAction(mSaveXMLAct);

	mEditMenu = menuBar()->addMenu(tr("&Edit"));
	mEditMenu->addAction(mAddAttributeAct);
	mEditMenu->addAction(mRemoveAttributeAct);

	mViewMenu = menuBar()->addMenu(tr("&View"));
	mViewMenu->addAction(mNormalViewAct);
	mViewMenu->addAction(mNodeViewAct);

	mHelpMenu = menuBar()->addMenu(tr("&Help"));
	mHelpMenu->addAction(mHelpAboutAct);

	/* toolbar */

	mFileToolbar = addToolBar(tr("File"));
	mFileToolbar->addAction(mLoadFragDbAct);
	mFileToolbar->addAction(mLoadMatchDbAct);
	mFileToolbar->addAction(mSaveDbAct);
	mFileToolbar->addSeparator();
	mFileToolbar->addAction(mAddAttributeAct);
	mFileToolbar->addAction(mRemoveAttributeAct);
	mFileToolbar->addSeparator();
	mFileToolbar->addAction(mImportXMLAct);
	mFileToolbar->addAction(mSaveXMLAct);
	mFileToolbar->addSeparator();
	mFileToolbar->addAction(mNormalViewAct);
	mFileToolbar->addAction(mNodeViewAct);

	mFileToolbar->setMovable(false);

	/* status bar */

	setStatusBar(new QStatusBar());

	mNumberOfMatchesLabel = new QLabel();
	//mNumberOfMatchesLabel->setText(MATCH_COUNT_TEXT.arg(0));

	statusBar()->addPermanentWidget(mNumberOfMatchesLabel);

	updateStatusBar();

	/* central widget */

	mCentralWidget = new QStackedWidget;

#ifdef WITH_TILEVIEW
	mTileView = new MatchTileView(mThumbDir);
	mTileView->setModel(&mModel);
	mTileView->setSelectionModel(mSelectionModel);
	mCentralWidget->addWidget(mTileView);

	mTileViewToolbar = addToolBar(tr("Tile View"));
	mTileViewToolbar->addActions(mTileView->actions());
	mTileViewToolbar->addActions(mTileView->toolbarOnlyActions());
	mTileViewToolbar->setMovable(false);

	mTileViewMenu = menuBar()->addMenu(tr("&Actions"));
	mTileViewMenu->addActions(mTileView->actions());

	foreach (QWidget *widget, mTileView->statusBarWidgets()) {
		statusBar()->addWidget(widget);
	}
#else
	mCentralWidget->addWidget(new QWidget);
#endif

#ifdef WITH_GRAPH
	mGraphView = new GraphView;
	mGraphView->setModel(&mModel);
	mCentralWidget->addWidget(mGraphView);

	mGraphViewToolbar = addToolBar(tr("Graph View"));
	mGraphViewToolbar->addActions(mGraphView->actions());
	mGraphViewToolbar->setMovable(false);

	mGraphViewMenu = menuBar()->addMenu(tr("&Actions"));
	mGraphViewMenu->addActions(mGraphView->actions());
#else
	mCentralWidget->addWidget(new QWidget);
#endif

	setCentralWidget(mCentralWidget);

	normalView();

	/* styles */
	/*
	QFile file(":/rcc/stylesheet.qss");
	file.open(QFile::ReadOnly);
	QString styleSheet = QLatin1String(file.readAll());
	file.close();

	setStyleSheet(styleSheet);
	*/

	/* window geometry */

	resize(MIN_WIDTH, MIN_HEIGHT);

	// center app on screen
	QRect desktopRect = QApplication::desktop()->screenGeometry();
	move(desktopRect.width() / 2 - width() / 2, desktopRect.height() / 2 - height() / 2);

	//setGeometry(screenSize);
	//QRect screenSize = (QApplication::desktop())->availableGeometry(this);
	//setGeometry(screenSize.x(), screenSize.y(), screenSize.width(), screenSize.height() - (height() - frameGeometry().height()));

	//showMaximized();

	//qDebug() << "frameGeometry:" << frameGeometry() << " | frameSize:" << frameSize();
}

void Tangerine::createActions() {
	mLoadFragDbAct = new QAction(QIcon(":/rcc/fatcow/32x32/folder_table.png"), tr("Load &fragment database"), this);
	mLoadFragDbAct->setStatusTip(tr("Select and load a fragment database"));
	connect(mLoadFragDbAct, SIGNAL(triggered()), this, SLOT(loadFragmentDatabase()));

	mLoadMatchDbAct = new QAction(QIcon(":/rcc/fatcow/32x32/folder_database.png"), tr("Load &match database"), this);
	mLoadMatchDbAct->setShortcuts(QKeySequence::Open);
	mLoadMatchDbAct->setStatusTip(tr("Select and load a match database"));
    connect(mLoadMatchDbAct, SIGNAL(triggered()), this, SLOT(loadMatchDatabase()));

    mSaveDbAct = new QAction(QIcon(":/rcc/fatcow/32x32/database_save.png"), tr("&Save database"), this);
    mSaveDbAct->setShortcuts(QKeySequence::Save);
    mSaveDbAct->setStatusTip(tr("Save the database in a (new) database file"));
	connect(mSaveDbAct, SIGNAL(triggered()), this, SLOT(saveDatabase()));

	mImportXMLAct = new QAction(QIcon(":/rcc/fatcow/32x32/page_go.png"), tr("&Import from XML"), this);
	mImportXMLAct->setStatusTip(tr("Select and import an XML file"));
	connect(mImportXMLAct, SIGNAL(triggered()), this, SLOT(importDatabase()));

	mSaveXMLAct = new QAction(QIcon(":/rcc/fatcow/32x32/page_save.png"), tr("&Export to XML"), this);
	mSaveXMLAct->setShortcuts(QKeySequence::SaveAs);
	mSaveXMLAct->setStatusTip(tr("Export the current database to an XML file"));
	connect(mSaveXMLAct, SIGNAL(triggered()), this, SLOT(exportDatabase()));

	mNormalViewAct = new QAction(QIcon(":/rcc/fatcow/32x32/things_digital.png"), tr("Switch to &normal view"), this);
	mNormalViewAct->setCheckable(true);
	mNormalViewAct->setStatusTip(tr("Switch to normal view"));
	connect(mNormalViewAct, SIGNAL(triggered()), this, SLOT(normalView()));

	mNodeViewAct = new QAction(QIcon(":/rcc/fatcow/32x32/bubblechart.png"), tr("Switch to n&ode view"), this);
	mNodeViewAct->setCheckable(true);
	mNodeViewAct->setStatusTip(tr("Switch to node view"));
	connect(mNodeViewAct, SIGNAL(triggered()), this, SLOT(nodeView()));

	mViewGroup = new QActionGroup(this);
	mViewGroup->addAction(mNormalViewAct);
	mViewGroup->addAction(mNodeViewAct);
	mNormalViewAct->setChecked(true);

	mAddAttributeAct = new QAction(QIcon(":/rcc/fatcow/32x32/cog_add.png"), tr("Add an attribute to the matches"), this);
	mAddAttributeAct->setStatusTip(tr("Add an attribute to the matches"));
	connect(mAddAttributeAct, SIGNAL(triggered()), this, SLOT(addAttribute()));

	mRemoveAttributeAct = new QAction(QIcon(":/rcc/fatcow/32x32/cog_delete.png"), tr("Remove an attribute from the matches"), this);
	mRemoveAttributeAct->setStatusTip(tr("Remove an attribute from the matches"));
	connect(mRemoveAttributeAct, SIGNAL(triggered()), this, SLOT(removeAttribute()));

    mHelpAboutAct = new QAction(QIcon(":/rcc/fatcow/32x32/information.png"), tr("&About"), this);
    mHelpAboutAct->setStatusTip(tr("Show the about dialog"));
	connect(mHelpAboutAct, SIGNAL(triggered()), this, SLOT(about()));
}

/**
 * TODO: fill in
 */
void Tangerine::closeDatabase() {
	/*
	if (mDb != NULL) {
		//delete mDb;

		mDb = NULL;
	}
	*/
}

void Tangerine::updateStatusBar() {
	mNumberOfMatchesLabel->setText(MATCH_COUNT_TEXT.arg(mDb.matchCount()));
}

void Tangerine::loadFragmentDatabase() {
	QSettings settings;

	QString dbDir = QFileDialog::getExistingDirectory(
		this,
		QObject::tr("Choose the fragment database root directory"),
		QString(),
		QFileDialog::ShowDirsOnly |
		QFileDialog::DontResolveSymlinks |
		QFileDialog::DontConfirmOverwrite
	);

	if (!dbDir.isEmpty() && Database::init(dbDir, Database::FRAGMENT, true)) {
		settings.setValue(SETTINGS_DB_ROOT_KEY, dbDir);

		emit fragmentDatabaseOpened();
	}
}


void Tangerine::loadMatchDatabase() {
	QString fileName = QFileDialog::getSaveFileName(this, tr("Open database file or make one"), QString(), QString(), 0, QFileDialog::DontConfirmOverwrite);

	if (fileName != "") {
		mDb.connect(fileName);

		if (!mDb.isOpen()) {
			QMessageBox::information(this, tr("Couldn't open database"), tr("Was unable to open database"));
		}
	}
}

void Tangerine::saveDatabase() {
	qDebug() << "dummy functionality";

	// what are we supposed to do here? the database is automatically saved no? maybe copy file to other location?
}

void Tangerine::importDatabase() {
	QString fileName = QFileDialog::getSaveFileName(this, tr("Choose an XML file to import"), QString(), tr("XML files (*.xml)"), 0, QFileDialog::DontConfirmOverwrite);

	if (fileName != "") {
		mDb.loadFromXML(fileName);
	}
}

void Tangerine::exportDatabase() {
	QString fileName = QFileDialog::getSaveFileName(this, tr("To which file do you want to export?"), QString(), tr("XML files (*.xml)"), 0);

	if (fileName != "") {
		mDb.saveToXML(fileName);
	}
}

void Tangerine::addAttribute() {
	bool ok;

	QString name = QInputDialog::getText(this, tr("Add attribute"), tr("Choose an attribute name"), QLineEdit::Normal, tr(""), &ok).toLower();

	if (ok && !name.isEmpty()) {
		QStringList typeList = QStringList() << "Text" << "Real" << "Integer";

		QString type = QInputDialog::getItem(this, tr("Add attribute"), tr("What type is the field?"), typeList, 0, false, &ok).toLower();

		if (ok && !type.isEmpty()) {
			QString defaultValue = QInputDialog::getText(this, tr("Add attribute"), tr("Input a default value"), QLineEdit::Normal, tr(""), &ok);

			if (ok && (!defaultValue.isEmpty() || type == "text")) {
				type = type.toLower();

				if (type == "text") {
					mModel.addField(name, defaultValue.isEmpty() ? QString() : defaultValue);
				}
				else if (type == "real") {
					mModel.addField(name, defaultValue.toDouble());
				}
				else if (type == "integer") {
					mModel.addField(name, defaultValue.toInt());
				}
				else {
					qDebug() << "Unknown type";
				}
			}
			else {
				qDebug() << "Didn't add attribute:" << type << ":" << defaultValue;
			}
		}
	}
}

void Tangerine::removeAttribute() {
	QStringList fieldList = QStringList::fromSet(mModel.fieldList());

	if (!fieldList.isEmpty()) {
		bool ok;

		QString field = QInputDialog::getItem(this, "Remove attribute", "Choose an attribute to remove", fieldList, 0, false, &ok);

		if (ok && !field.isEmpty()) {
			mModel.removeField(field);
		}
	}
}

void Tangerine::normalView() {
#ifdef WITH_TILEVIEW
	mTileViewMenu->menuAction()->setVisible(true);
	mTileViewToolbar->setVisible(true);

	foreach (QWidget *widget, mTileView->statusBarWidgets()) {
		widget->setVisible(true);
	}
#endif

#ifdef WITH_GRAPH
	mGraphViewMenu->menuAction()->setVisible(false);
	mGraphViewToolbar->setVisible(false);
#endif

	mCentralWidget->setCurrentIndex(0);
}

void Tangerine::nodeView() {
#ifdef WITH_TILEVIEW
	mTileViewMenu->menuAction()->setVisible(false);
	mTileViewToolbar->setVisible(false);

	foreach (QWidget *widget, mTileView->statusBarWidgets()) {
		widget->setVisible(false);
	}
#endif

#ifdef WITH_GRAPH
	mGraphViewMenu->menuAction()->setVisible(true);
	mGraphViewToolbar->setVisible(true);
#endif

	mCentralWidget->setCurrentIndex(1);
}

void Tangerine::fragmentDatabaseOpened() {
	mLoadFragDbAct->setEnabled(false);
	mLoadMatchDbAct->setEnabled(true);
	mImportXMLAct->setEnabled(true);
}

void Tangerine::matchCountChanged() {
	updateStatusBar();
}

void Tangerine::databaseOpened() {
	mSaveDbAct->setEnabled(true);
	mSaveXMLAct->setEnabled(true);
	mAddAttributeAct->setEnabled(true);
	mRemoveAttributeAct->setEnabled(true);
}

void Tangerine::databaseClosed() {
	mSaveDbAct->setEnabled(false);
	mSaveXMLAct->setEnabled(false);
	mAddAttributeAct->setEnabled(false);
	mRemoveAttributeAct->setEnabled(false);
}

void Tangerine::databaseOpStarted(const QString& operation, int steps) {
	if (mProgress != NULL) {
		delete mProgress;

		mProgress = NULL;
	}

	mProgress = new QProgressDialog(operation, QString(), 0, steps, this);
	mProgress->setMinimumWidth(400);
	mProgress->setMinimumDuration(0);
	mProgress->setWindowModality(Qt::WindowModal);
	mProgress->show();
}

void Tangerine::databaseOpStepDone(int step) {
	QApplication::processEvents();

	if (mProgress != NULL) {
		mProgress->setValue(step);
	}
}

void Tangerine::databaseOpEnded() {
	if (mProgress != NULL) {
		mProgress->setValue(mProgress->maximum());

		delete mProgress;

		mProgress = NULL;
	}
}

void Tangerine::about() {
	QMessageBox::about(
		this,
		tr("About Tangerine"),
		tr("<b>Tangerine</b> is a next-generation proof of concept GUI for the <b>Thera project</b>. It intends to aid the user in finding and confirming fragment matches.")
	);
}
