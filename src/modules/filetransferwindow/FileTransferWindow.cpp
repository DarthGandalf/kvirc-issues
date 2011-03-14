//=============================================================================
//
//   File : FileTransferWindow.cpp
//   Creation date : Mon Apr 21 2003 23:14:12 CEST by Szymon Stefanek
//
//   This file is part of the KVIrc irc client distribution
//   Copyright (C) 2003-2010 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the linkss of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//=============================================================================

#include "FileTransferWindow.h"

#include "kvi_debug.h"
#include "KviIconManager.h"
#include "KviIrcView.h"
#include "kvi_out.h"
#include "KviOptions.h"
#include "KviLocale.h"
#include "kvi_out.h"
#include "KviThemedLabel.h"
#include "KviInput.h"
#include "KviQString.h"
#include "KviTalHBox.h"
#include "KviMainWindow.h"
#include "KviMdiManager.h"

#include <QToolTip>
#include <QPainter>
#include <QMessageBox>
#include <QClipboard>
#include <QFileInfo>
#include <QFile>
#include <QLabel>
#include <QFontMetrics>
#include <QEvent>
#include <QKeyEvent>

#include <QHeaderView>

#ifdef COMPILE_KDE_SUPPORT
	#include <kurl.h>
	#include <krun.h>
	#include <kmimetype.h>
	#include <kmimetypetrader.h>
	#include <kiconloader.h>
#endif //COMPILE_KDE_SUPPORT

#ifdef COMPILE_PSEUDO_TRANSPARENCY
	extern KVIRC_API QPixmap * g_pShadedChildGlobalDesktopBackground;
#endif

#define FILETRANSFERW_CELLSIZE 70

extern FileTransferWindow * g_pFileTransferWindow;


FileTransferItem::FileTransferItem(FileTransferWidget * v,KviFileTransfer * t)
: KviTalTableWidgetItemEx(v)
{
	m_pTransfer = t;
	m_pTransfer->setDisplayItem(this);

	//create items for the second and the third column
	col1Item = new KviTalTableWidgetItem(v, row(), 1);
	col2Item = new KviTalTableWidgetItem(v, row(), 2);
	//FIXME fixed row height
	tableWidget()->setRowHeight( row(), FILETRANSFERW_CELLSIZE );
}

FileTransferItem::~FileTransferItem()
{
	m_pTransfer->setDisplayItem(0);
	delete col1Item;
	delete col2Item;
}

void FileTransferItem::displayUpdate()
{
	//FIXME this hurts
	int dummy = (int) time(NULL);
	tableWidget()->model()->setData(tableWidget()->model()->index(row(),0), dummy, Qt::DisplayRole);
	tableWidget()->model()->setData(tableWidget()->model()->index(row(),1), dummy, Qt::DisplayRole);
	tableWidget()->model()->setData(tableWidget()->model()->index(row(),2), dummy, Qt::DisplayRole);
}

QString FileTransferItem::key(int,bool) const
{
	QString ret;
	ret.setNum(m_pTransfer->id());
	if(ret.length() == 1)ret.prepend("0000000");
	else if(ret.length() == 2)ret.prepend("000000");
	else if(ret.length() == 3)ret.prepend("00000");
	else if(ret.length() == 4)ret.prepend("0000");
	else if(ret.length() == 5)ret.prepend("000");
	else if(ret.length() == 6)ret.prepend("00");
	else if(ret.length() == 7)ret.prepend("0");
	return ret;
}



FileTransferWidget::FileTransferWidget(QWidget * pParent)
: KviTalTableWidget(pParent)
{
	//hide the header
	verticalHeader()->hide();
	//hide cells grids
	setShowGrid(false);
	//disable cell content editing
	setEditTriggers(QAbstractItemView::NoEditTriggers);

	//select one row at once
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);

	//prepare columns
	setColumnCount(3);

	QStringList colHeaders;
	colHeaders << __tr2qs_ctx("Type","filetransferwindow")
				<< __tr2qs_ctx("Information","filetransferwindow")
				<< __tr2qs_ctx("Progress","filetransferwindow");
	setHorizontalHeaderLabels(colHeaders);
	//default column widths
	setColumnWidth(0, FILETRANSFERW_CELLSIZE);
	horizontalHeader()->setResizeMode(0, QHeaderView::Fixed);
	horizontalHeader()->setResizeMode(1, QHeaderView::Interactive);
	setColumnWidth(1, 500);
	horizontalHeader()->setStretchLastSection(true);
	//focus policy
	setFocusPolicy(Qt::NoFocus);
	viewport()->setFocusPolicy(Qt::NoFocus);
}

void FileTransferWidget::mousePressEvent (QMouseEvent *e)
{
	if (e->button() == Qt::RightButton)
	{
		// we have 3 items: the one on the first column is the real one (has a transfer() member)
		// others are just fakes
		QTableWidgetItem *clicked = itemAt(e->pos());
		if(clicked)
		{
			FileTransferItem *i = (FileTransferItem*) item(clicked->row(),0);
			if (i) emit rightButtonPressed(i, QCursor::pos());
		}
	}
	QTableWidget::mousePressEvent(e);
}

void FileTransferWidget::mouseDoubleClickEvent (QMouseEvent *e)
{
	// we have 3 items: the one on the first column is the real one (has a transfer() member)
	// others are just fakes
	QTableWidgetItem *clicked = itemAt(e->pos());
	if(clicked)
	{
		FileTransferItem *i = (FileTransferItem*) item(clicked->row(),0);
		if (i) emit doubleClicked(i,QCursor::pos());
	}
	QTableWidget::mouseDoubleClickEvent(e);
}

void FileTransferWidget::paintEvent(QPaintEvent * event)
{
	QPainter *p = new QPainter(viewport());
	QStyleOptionViewItem option = viewOptions();
	QRect rect = event->rect();

#ifdef COMPILE_PSEUDO_TRANSPARENCY
	if(KVI_OPTION_BOOL(KviOption_boolUseCompositingForTransparency) && g_pApp->supportsCompositing())
	{
		p->save();
		p->setCompositionMode(QPainter::CompositionMode_Source);
		QColor col=KVI_OPTION_COLOR(KviOption_colorGlobalTransparencyFade);
		col.setAlphaF((float)((float)KVI_OPTION_UINT(KviOption_uintGlobalTransparencyChildFadeFactor) / (float)100));
		p->fillRect(rect, col);
		p->restore();
	} else if(g_pShadedChildGlobalDesktopBackground)
	{
		QPoint pnt = g_pFileTransferWindow->mdiParent() ? viewport()->mapTo(g_pMainWindow, rect.topLeft() + g_pMainWindow->mdiManager()->scrollBarsOffset()) : viewport()->mapTo(g_pFileTransferWindow, rect.topLeft());
		p->drawTiledPixmap(rect,*(g_pShadedChildGlobalDesktopBackground), pnt);
	} else {
#endif
		//FIXME this is not the treewindowlist
		p->fillRect(rect,KVI_OPTION_COLOR(KviOption_colorTreeWindowListBackground));
#ifdef COMPILE_PSEUDO_TRANSPARENCY
	}
#endif
	delete p;

	//call paint on all childrens
	KviTalTableWidget::paintEvent(event);
}

void FileTransferItemDelegate::paint(QPainter * p, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	//FIXME not exactly model/view coding style.. but we need to access data on the item by now
	FileTransferWidget* tableWidget = (FileTransferWidget*)parent();
	FileTransferItem* item = (FileTransferItem*) tableWidget->itemFromIndex(index);

	if(!item) return;
	KviFileTransfer* transfer = ((FileTransferItem*)tableWidget->item(item->row(), 0))->transfer();


	p->setFont(option.font);

	p->setPen(option.state & QStyle::State_Selected ? option.palette.highlight().color() : option.palette.base().color());

	p->drawRect(option.rect);
	p->setPen(transfer->active() ? QColor(180,180,180) : QColor(200,200,200));

	p->drawRect(option.rect.left() + 1, option.rect.top() + 1, option.rect.width() - 2,option.rect.height() - 2);
	p->fillRect(option.rect.left() + 2, option.rect.top() + 2, option.rect.width() - 4, option.rect.height() - 4,transfer->active() ? QColor(240,240,240) : QColor(225,225,225));

	transfer->displayPaint(p, index.column(), option.rect);
}

QSize FileTransferItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
	// FIXME fixed width
	return QSize(((FileTransferWidget*)parent())->viewport()->size().width(), 68);
}

FileTransferWindow::FileTransferWindow(KviModuleExtensionDescriptor * d,KviMainWindow * lpFrm)
: KviWindow(KviWindow::Tool,lpFrm,"file transfer window",0), KviModuleExtension(d)
{
	g_pFileTransferWindow = this;
	setAutoFillBackground(false);
	
	m_pContextPopup = 0;
	m_pLocalFilePopup = 0;
	m_pOpenFilePopup = 0;

	m_pTimer = new QTimer(this);
	connect(m_pTimer,SIGNAL(timeout()),this,SLOT(heartbeat()));

	m_pSplitter = new KviTalSplitter(Qt::Horizontal,this);
	m_pSplitter->setObjectName("transferwindow_hsplitter");
	m_pSplitter->setChildrenCollapsible(false);

	m_pVertSplitter = new KviTalSplitter(Qt::Vertical,m_pSplitter);
	m_pVertSplitter->setObjectName("transferwindow_vsplitter");
	m_pVertSplitter->setChildrenCollapsible(false);

	m_pTableWidget  = new FileTransferWidget(m_pVertSplitter);

	//ad-hoc itemdelegate for this view
	m_pItemDelegate = new FileTransferItemDelegate(m_pTableWidget);
	m_pTableWidget->setItemDelegate(m_pItemDelegate);

/*
 * TODO
	KviDynamicToolTip * tp = new KviDynamicToolTip(m_pTableWidget->viewport());
	connect(tp,SIGNAL(tipRequest(KviDynamicToolTip *,const QPoint &)),this,SLOT(tipRequest(KviDynamicToolTip *,const QPoint &)));
*/

	QFontMetrics fm(font());
	m_iLineSpacing = fm.lineSpacing();

	m_pIrcView = new KviIrcView(m_pVertSplitter,lpFrm,this);

	m_pTableWidget->installEventFilter(this);

	connect(m_pTableWidget,SIGNAL(rightButtonPressed(FileTransferItem *,QPoint)),
		this,SLOT(rightButtonPressed(FileTransferItem *,QPoint)));
	connect(m_pTableWidget,SIGNAL(doubleClicked(FileTransferItem *,const QPoint &)),this,SLOT(doubleClicked(FileTransferItem *,const QPoint &)));
	fillTransferView();

	connect(KviFileTransferManager::instance(),SIGNAL(transferRegistered(KviFileTransfer *)),this,SLOT(transferRegistered(KviFileTransfer *)));
	connect(KviFileTransferManager::instance(),SIGNAL(transferUnregistering(KviFileTransfer *)),this,SLOT(transferUnregistering(KviFileTransfer *)));

	KviFileTransferManager::instance()->setTransferWindow(this);

	m_pTimer->start(2000);
}

FileTransferWindow::~FileTransferWindow()
{
	KviFileTransferManager::instance()->setTransferWindow(0);
	g_pFileTransferWindow = 0;
}

bool FileTransferWindow::eventFilter( QObject *obj, QEvent *ev )
{
	if( ( ev->type() == QEvent::KeyPress ) )
	{
		//m_pTableWidget could not have been initialized yet when FileTransferWindow
		//will receive the first events.. but it will surely be on a keypress event
		if(obj==m_pTableWidget)
		{
			QKeyEvent *keyEvent = (QKeyEvent*)ev;
			switch(keyEvent->key())
			{
				case Qt::Key_Delete:
					if(m_pTableWidget->currentItem())
					{
						delete m_pTableWidget->currentItem();
						return true;
					}
					break;
			}
		}
	}
	return KviWindow::eventFilter( obj, ev );
}

void FileTransferWindow::fontChange(const QFont &oldFont)
{
	QFontMetrics fm(font());
	m_iLineSpacing = fm.lineSpacing();
	KviWindow::fontChange(oldFont);
}

void FileTransferWindow::tipRequest(KviDynamicToolTip * tip,const QPoint &pnt)
{
	FileTransferItem * it = (FileTransferItem *)m_pTableWidget->itemAt(pnt);
	if(!it)return;
	QString txt = it->transfer()->tipText();
	tip->tip(m_pTableWidget->visualItemRect(it),txt);
}

void FileTransferWindow::fillTransferView()
{
	KviPointerList<KviFileTransfer> * l = KviFileTransferManager::instance()->transferList();
	if(!l)return;
	FileTransferItem * it;
	for(KviFileTransfer * t = l->first();t;t = l->next())
	{
		it = new FileTransferItem(m_pTableWidget,t);
		t->setDisplayItem(it);
	}
}

FileTransferItem * FileTransferWindow::findItem(KviFileTransfer * t)
{
	int i;
	FileTransferItem * it;

	for(i=0;i<m_pTableWidget->rowCount();i++)
	{
		it = (FileTransferItem *)m_pTableWidget->item(i,0);
		if(!it) continue;

		if(it->transfer() == t)
			return it;
	}
	return 0;
}

void FileTransferWindow::transferRegistered(KviFileTransfer * t)
{
	new FileTransferItem(m_pTableWidget,t);
	//t->setDisplayItem(it);
}

void FileTransferWindow::transferUnregistering(KviFileTransfer * t)
{
	FileTransferItem * it = findItem(t);
	//t->setDisplayItem(0);
	if(it)
	{
		delete it;
		it = 0;
	}
}

void FileTransferWindow::doubleClicked(FileTransferItem *it,const QPoint &)
{
	if(it) openLocalFile();
}

void FileTransferWindow::rightButtonPressed(FileTransferItem *it,const QPoint &pnt)
{
	if(!m_pContextPopup)m_pContextPopup = new KviTalPopupMenu(this);
	if(!m_pLocalFilePopup)m_pLocalFilePopup = new KviTalPopupMenu(this);
	if(!m_pOpenFilePopup)
	{
		m_pOpenFilePopup= new KviTalPopupMenu(this);
		connect(m_pOpenFilePopup,SIGNAL(activated(int)),this,SLOT(openFilePopupActivated(int)));
	}

	m_pContextPopup->clear();

	int id;

	if(it)
	{
		FileTransferItem * i = (FileTransferItem *)it;
		if(i->transfer())
		{

			QString szFile = i->transfer()->localFileName();
			if(!szFile.isEmpty())
			{
				m_pLocalFilePopup->clear();

				QString tmp = "<b>file:/";
				tmp += szFile;
				tmp += "</b><br>";

				QFileInfo fi(szFile);
				if(fi.exists())
				{
					tmp += "<nobr>";
					tmp += __tr2qs_ctx("Size: %1","filetransferwindow").arg(KviQString::makeSizeReadable(fi.size()));
					tmp += "</nobr><br>";
				}

#ifdef COMPILE_KDE_SUPPORT
				tmp += "<nobr>Mime: ";
				tmp += KMimeType::findByPath(szFile)->name();
				tmp += "</nobr>";
#endif //COMPILE_KDE_SUPPORT

				QLabel * l = new QLabel(tmp,m_pLocalFilePopup);
				QPalette p;
				l->setStyleSheet("background-color: " + p.color(QPalette::Normal, QPalette::Mid).name());
				m_pLocalFilePopup->insertItem(l);

#ifdef COMPILE_KDE_SUPPORT
				QString mimetype = KMimeType::findByPath(szFile)->name();
				KService::List offers = KMimeTypeTrader::self()->query(mimetype,"Application");

				id = m_pLocalFilePopup->insertItem(__tr2qs_ctx("&Open","filetransferwindow"),this,SLOT(openLocalFile()));
				m_pLocalFilePopup->setItemParameter(id,-1);

				m_pOpenFilePopup->clear();

				int id;
				int idx = 0;

				for(KService::List::Iterator itOffers = offers.begin();
	   				itOffers != offers.end(); ++itOffers)
				{
					id = m_pOpenFilePopup->insertItem(
							SmallIcon((*itOffers).data()->icon()),
							(*itOffers).data()->name()
						);
					m_pOpenFilePopup->setItemParameter(id,idx);
					idx++;
				}

				m_pOpenFilePopup->insertSeparator();

				id = m_pOpenFilePopup->insertItem(__tr2qs_ctx("&Other...","filetransferwindow"),this,SLOT(openLocalFileWith()));
				m_pOpenFilePopup->setItemParameter(id,-1);

				m_pLocalFilePopup->insertItem(__tr2qs_ctx("Open &With","filetransferwindow"),m_pOpenFilePopup);
				m_pLocalFilePopup->insertSeparator();
				m_pLocalFilePopup->insertItem(__tr2qs_ctx("Open &Location","filetransferwindow"),this,SLOT(openLocalFileFolder()));
				m_pLocalFilePopup->insertItem(__tr2qs_ctx("Terminal at Location","filetransferwindow"),this,SLOT(openLocalFileTerminal()));
				m_pLocalFilePopup->insertSeparator();
#endif //COMPILE_KDE_SUPPORT

//-| Grifisx & Noldor |-
#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)
				id = m_pLocalFilePopup->insertItem(__tr2qs_ctx("&Open","filetransferwindow"),this,SLOT(openLocalFile()));
				m_pLocalFilePopup->setItemParameter(id,-1);
				m_pOpenFilePopup->insertSeparator();
				m_pLocalFilePopup->insertItem(__tr2qs_ctx("Open &With","filetransferwindow"),this,SLOT(openLocalFileWith()));
				m_pLocalFilePopup->insertSeparator();
				m_pLocalFilePopup->insertItem(__tr2qs_ctx("Open &Location","filetransferwindow"),this,SLOT(openLocalFileFolder()));
				m_pLocalFilePopup->insertItem(__tr2qs_ctx("MS-DOS Prompt at Location","filetransferwindow"),this,SLOT(openLocalFileTerminal()));
				m_pLocalFilePopup->insertSeparator();
#endif
// G&N end

				m_pLocalFilePopup->insertItem(__tr2qs_ctx("&Copy Path to Clipboard","filetransferwindow"),this,SLOT(copyLocalFileToClipboard()));

				id = m_pLocalFilePopup->insertItem(__tr2qs_ctx("&Delete file","filetransferwindow"),this,SLOT(deleteLocalFile()));
				m_pLocalFilePopup->setItemEnabled(id,i->transfer()->terminated());
				m_pContextPopup->insertItem(__tr2qs_ctx("Local &File","filetransferwindow"),m_pLocalFilePopup);


			}

			i->transfer()->fillContextPopup(m_pContextPopup);
			m_pContextPopup->insertSeparator();
		}
	}


	bool bHaveTerminated = false;
	int i;
	FileTransferItem * item;

	for(i=0;i<m_pTableWidget->rowCount();i++)
	{
		item = (FileTransferItem *)m_pTableWidget->item(i,0);
		if(!item)
			continue;

		if(item->transfer()->terminated())
		{
			bHaveTerminated = true;
			break;
		}
	}

	id = m_pContextPopup->insertItem(__tr2qs_ctx("&Clear Terminated","filetransferwindow"),this,SLOT(clearTerminated()));
	m_pContextPopup->setItemEnabled(id,bHaveTerminated);

	bool bAreTransfersActive = false;
	if(m_pTableWidget->rowCount() >= 1)
		bAreTransfersActive = true;

	id = m_pContextPopup->insertItem(__tr2qs_ctx("Clear &All","filetransferwindow"),this,SLOT(clearAll()));
	m_pContextPopup->setItemEnabled(id,bAreTransfersActive);

	m_pContextPopup->popup(pnt);
}


KviFileTransfer * FileTransferWindow::selectedTransfer()
{
	if(m_pTableWidget->selectedItems().count() == 0) return 0;
	FileTransferItem * it = (FileTransferItem *)m_pTableWidget->selectedItems().first();
	if(!it)return 0;
	FileTransferItem * i = (FileTransferItem *)it;
	return i->transfer();
}

void FileTransferWindow::openFilePopupActivated(int id)
{
#ifdef COMPILE_KDE_SUPPORT
	int ip = m_pOpenFilePopup->itemParameter(id);
	if(ip < 0)return;

	KviFileTransfer * t = selectedTransfer();
	if(!t)return;
	QString tmp = t->localFileName();
	if(tmp.isEmpty())return;


	QString mimetype = KMimeType::findByPath(tmp)->name();
	KService::List offers = KMimeTypeTrader::self()->query(mimetype,"Application");
	
	int idx = 0;
	for(KService::List::Iterator itOffers = offers.begin();
	   				itOffers != offers.end(); ++itOffers)
	{
		if(idx == ip)
		{
			KUrl::List lst;
			KUrl url;
			url.setPath(tmp);
			lst.append(url);
			KRun::run(*((*itOffers).data()),lst,g_pMainWindow);
			break;
		}
		idx++;
	}
#endif //COMPILE_KDE_SUPPORT
}

void FileTransferWindow::openLocalFileTerminal()
{
//-| Grifisx & Noldor |-
#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)
	KviFileTransfer * t = selectedTransfer();
	if(!t)return;
	QString tmp = t->localFileName();
	if(tmp.isEmpty())return;
	int idx = tmp.lastIndexOf("/");
	if(idx == -1)return;
	tmp = tmp.left(idx);
	tmp.append("\"");
	// FIXME: this is not a solution ...because if the drive isn't system's drive the command 'cd' naturaly doesn't work
	tmp.prepend("cmd.exe /k cd \"");
	system(tmp.toLocal8Bit().data());
#else
// G&N end
	#ifdef COMPILE_KDE_SUPPORT
		KviFileTransfer * t = selectedTransfer();
		if(!t)return;
		QString tmp = t->localFileName();
		if(tmp.isEmpty())return;

		int idx = tmp.lastIndexOf("/");
		if(idx == -1)return;
		tmp = tmp.left(idx);

		tmp.prepend("konsole --workdir=\"");
		tmp.append("\"");

		KRun::runCommand(tmp,g_pMainWindow);
	#endif //COMPILE_KDE_SUPPORT
#endif
}

void FileTransferWindow::deleteLocalFile()
{
	KviFileTransfer * pTransfer = selectedTransfer();
	if(!pTransfer)
		return;

	QString szName = pTransfer->localFileName();
	QString szTmp = QString(__tr2qs_ctx("Do you really want to delete the file %1?","filetransferwindow")).arg(szName);

	if(QMessageBox::warning(this,__tr2qs_ctx("Confirm delete","filetransferwindow"),
			szTmp,__tr2qs_ctx("Yes","filetransferwindow"),__tr2qs_ctx("No","filetransferwindow")) != 0)
		return;

	if(!QFile::remove(szName))
		QMessageBox::warning(this,__tr2qs_ctx("Delete failed","filetransferwindow"),
			__tr2qs_ctx("Failed to remove the file","filetransferwindow"),
			__tr2qs_ctx("OK","filetransferwindow"));
}

void FileTransferWindow::openLocalFile()
{
//-| Grifisx & Noldor |-
#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)

	KviFileTransfer * t = selectedTransfer();
	if(!t)return;
	QString tmp = t->localFileName();
	if(tmp.isEmpty())return;
	tmp.replace("/","\\");
	ShellExecute(0,"open",tmp.toLocal8Bit().data(),NULL,NULL,SW_SHOWNORMAL);  //You have to link the shell32.lib
#else
// G&N end
	#ifdef COMPILE_KDE_SUPPORT
		KviFileTransfer * t = selectedTransfer();
		if(!t)return;
		QString tmp = t->localFileName();
		if(tmp.isEmpty())return;

		QString mimetype = KMimeType::findByPath(tmp)->name();  //KMimeType
		KService::Ptr offer = KMimeTypeTrader::self()->preferredService(mimetype,"Application");
		if(!offer)
		{
			openLocalFileWith();
			return;
		}

		KUrl::List lst;
		KUrl url;
		url.setPath(tmp);
		lst.append(url);

		KRun::run(*offer, lst, g_pMainWindow);
	#endif //COMPILE_KDE_SUPPORT
#endif
}

void FileTransferWindow::openLocalFileWith()
{
//-| Grifisx & Noldor |-
#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)
	KviFileTransfer * t = selectedTransfer();
	if(!t)return;
	QString tmp = t->localFileName();
	if(tmp.isEmpty())return;
	tmp.replace("/","\\");
	tmp.prepend("rundll32.exe shell32.dll,OpenAs_RunDLL "); // this if to show the 'open with...' window
	WinExec(tmp.toLocal8Bit().data(),SW_SHOWNORMAL);
#else
// G&N end
	#ifdef COMPILE_KDE_SUPPORT
		KviFileTransfer * t = selectedTransfer();
		if(!t)return;
		QString tmp = t->localFileName();
		if(tmp.isEmpty())return;

		KUrl::List lst;
		KUrl url;
		url.setPath(tmp);
		lst.append(url);
		KRun::displayOpenWithDialog(lst,g_pMainWindow);
	#endif //COMPILE_KDE_SUPPORT
#endif
}

void FileTransferWindow::copyLocalFileToClipboard()
{
	KviFileTransfer * t = selectedTransfer();
	if(!t)return;
	QString tmp = t->localFileName();
	if(tmp.isEmpty())return;
	QApplication::clipboard()->setText(tmp);
}

void FileTransferWindow::openLocalFileFolder()
{
//-| Grifisx & Noldor|-
#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)
	KviFileTransfer * t = selectedTransfer();
	if(!t)return;
	QString tmp = t->localFileName();
	if(tmp.isEmpty())return;
	tmp=QFileInfo(tmp).absolutePath();
	tmp.replace('/','\\');
	tmp.prepend("explorer.exe ");
	WinExec(tmp.toLocal8Bit().data(), SW_MAXIMIZE);
#else
// G&N end
	#ifdef COMPILE_KDE_SUPPORT
		KviFileTransfer * t = selectedTransfer();
		if(!t)return;
		QString tmp = t->localFileName();
		if(tmp.isEmpty())return;

		int idx = tmp.lastIndexOf("/");
		if(idx == -1)return;
		tmp = tmp.left(idx);

		QString mimetype = KMimeType::findByPath(tmp)->name(); // inode/directory
		KService::Ptr offer = KMimeTypeTrader::self()->preferredService(mimetype,"Application");
		if(!offer)return;

		KUrl::List lst;
		KUrl url;
		url.setPath(tmp);
		lst.append(url);
		KRun::run(*offer,lst,g_pMainWindow);
	#endif //COMPILE_KDE_SUPPORT
#endif
}

void FileTransferWindow::heartbeat()
{
	int i;
	QModelIndex index;
	FileTransferItem* it;
	int dummy = (int) time(NULL);

	for(i=0;i<=m_pTableWidget->rowCount();i++)
	{
		it = (FileTransferItem *)m_pTableWidget->item(i,0);

		if(!it)
			continue;

		if(it->transfer()->active())
		{
			m_pTableWidget->model()->setData(m_pTableWidget->model()->index(i,0), dummy, Qt::DisplayRole);
			m_pTableWidget->model()->setData(m_pTableWidget->model()->index(i,1), dummy, Qt::DisplayRole);
			m_pTableWidget->model()->setData(m_pTableWidget->model()->index(i,2), dummy, Qt::DisplayRole);
		}
	}
}

void FileTransferWindow::clearAll()
{
	bool bHaveAllTerminated = true;
	int i;
	FileTransferItem * pItem;

	for(i=0; i < m_pTableWidget->rowCount(); i++)
	{
		pItem = (FileTransferItem *)m_pTableWidget->item(i,0);
		if(!pItem)
			continue;

		if(!pItem->transfer()->terminated())
		{
			bHaveAllTerminated = false;
			break;
		}
	}

	QString szTmp = __tr2qs_ctx("Clear all transfers, including any in progress?","filetransferwindow");

	// If any transfer is active asks for confirm
	if(!bHaveAllTerminated)
		if(QMessageBox::warning(this,__tr2qs_ctx("Clear All Transfers? - KVIrc","filetransferwindow"), szTmp,__tr2qs_ctx("Yes","filetransferwindow"),__tr2qs_ctx("No","filetransferwindow")) != 0)
			return;

	KviFileTransferManager::instance()->killAllTransfers();
}

void FileTransferWindow::clearTerminated()
{
	KviFileTransferManager::instance()->killTerminatedTransfers();
}

void FileTransferWindow::getBaseLogFileName(QString &buffer)
{
	buffer.sprintf("FILETRANSFER");
}

QPixmap * FileTransferWindow::myIconPtr()
{
	return g_pIconManager->getSmallIcon(KviIconManager::FileTransfer);
}

void FileTransferWindow::resizeEvent(QResizeEvent *)
{
	m_pSplitter->setGeometry(0,0,width(),height());
}

QSize FileTransferWindow::sizeHint() const
{
	return m_pSplitter->sizeHint();
}

void FileTransferWindow::fillCaptionBuffers()
{
	m_szPlainTextCaption = __tr2qs_ctx("File Transfers","filetransferwindow");
}

void FileTransferWindow::die()
{
	close();
}



//#warning "Load & save properties of this kind of window"

//void FileTransferWindow::saveProperties()
//{
//	KviWindowProperty p;
//	p.rect = externalGeometry();
//	p.isDocked = isAttacched();
//	QValueList<int> l(m_pSplitter->sizes());
//	if(l.count() >= 1)p.splitWidth1 = *(l.at(0));
//	if(l.count() >= 2)p.splitWidth2 = *(l.at(1));
//	p.timestamp = m_pView->timestamp();
//	p.imagesVisible = m_pView->imagesVisible();
//	p.isMaximized = isAttacched() && isMaximized();
//	p.topSplitWidth1 = 0;
//	p.topSplitWidth2 = 0;
//	p.topSplitWidth3 = 0;
//	g_pOptions->m_pWinPropertiesList->setProperty(caption(),&p);
//}
//
//void FileTransferWindow::setProperties(KviWindowProperty *p)
//{
//	QValueList<int> l;
//	l.append(p->splitWidth1);
//	l.append(p->splitWidth2);
//	m_pVertSplitter->setSizes(l);
//	m_pIrcView->setTimestamp(p->timestamp);
//	m_pIrcView->setShowImages(p->imagesVisible);
//}

void FileTransferWindow::applyOptions()
{
	m_pIrcView->applyOptions();
	KviWindow::applyOptions();
}

#ifndef COMPILE_USE_STANDALONE_MOC_SOURCES
#include "m_FileTransferWindow.moc"
#endif //!COMPILE_USE_STANDALONE_MOC_SOURCES