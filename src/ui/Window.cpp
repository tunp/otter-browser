#include "Window.h"
#include "ui_Window.h"

namespace Otter
{

Window::Window(QWidget *parent) : QWidget(parent),
	m_ui(new Ui::Window)
{
	m_ui->setupUi(this);

	connect(m_ui->lineEdit, SIGNAL(returnPressed()), this, SLOT(loadUrl()));
	connect(m_ui->webView, SIGNAL(titleChanged(const QString)), this, SIGNAL(titleChanged(const QString)));
	connect(m_ui->webView, SIGNAL(urlChanged(const QUrl)), this, SLOT(notifyUrlChanged(const QUrl)));
	connect(m_ui->webView, SIGNAL(iconChanged()), this, SLOT(notifyIconChanged()));
}

Window::~Window()
{
	delete m_ui;
}

void Window::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);

	switch (event->type())
	{
		case QEvent::LanguageChange:
			m_ui->retranslateUi(this);

			break;
		default:
			break;
	}
}

void Window::undo()
{
	m_ui->webView->page()->undoStack()->undo();
}

void Window::redo()
{
	m_ui->webView->page()->undoStack()->redo();
}

void Window::cut()
{
	m_ui->webView->page()->triggerAction(QWebPage::Cut);
}

void Window::copy()
{
	m_ui->webView->page()->triggerAction(QWebPage::Copy);
}

void Window::paste()
{
	m_ui->webView->page()->triggerAction(QWebPage::Paste);
}

void Window::remove()
{
	m_ui->webView->page()->triggerAction(QWebPage::DeleteEndOfWord);
}

void Window::selectAll()
{
	m_ui->webView->page()->triggerAction(QWebPage::SelectAll);
}

void Window::zoomIn()
{
	m_ui->webView->setZoomFactor(qMin((m_ui->webView->zoomFactor() + 0.1), (qreal) 100));
}

void Window::zoomOut()
{
	m_ui->webView->setZoomFactor(qMax((m_ui->webView->zoomFactor() - 0.1), 0.1));
}

void Window::zoomOriginal()
{
	m_ui->webView->setZoomFactor(1);
}

void Window::setZoom(int zoom)
{
	m_ui->webView->setZoomFactor(qBound(0.1, ((qreal) zoom / 100), (qreal) 100));
}

void Window::setUrl(const QUrl &url)
{
	m_ui->webView->setUrl(url);
}

void Window::loadUrl()
{
	setUrl(QUrl(m_ui->lineEdit->text()));
}

void Window::notifyUrlChanged(const QUrl &url)
{
	m_ui->lineEdit->setText(url.toString());

	emit urlChanged(url);
}

void Window::notifyIconChanged()
{
	emit iconChanged(m_ui->webView->icon());
}

QWidget *Window::getDocument()
{
	return m_ui->webView;
}

QUndoStack *Window::getUndoStack()
{
	return m_ui->webView->page()->undoStack();
}

QString Window::getTitle() const
{
	const QString title = m_ui->webView->title();

	return (title.isEmpty() ? tr("New Tab") : title);
}

QUrl Window::getUrl() const
{
	return m_ui->webView->url();
}

QIcon Window::getIcon() const
{
	return m_ui->webView->icon();
}

int Window::getZoom() const
{
	return (m_ui->webView->zoomFactor() * 100);
}

}
