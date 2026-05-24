#include "process_terminal.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QProcess>
#include <QKeySequence>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextOption>

#include <algorithm>

ProcessTerminalWidget::ProcessTerminalWidget(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setUndoRedoEnabled(false);
    setWordWrapMode(QTextOption::NoWrap);
    setTabChangesFocus(false);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setBackgroundVisible(true);
    setCursorWidth(2);
    QFont mono("JetBrains Mono");
    if (!mono.exactMatch())
    {
        mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    mono.setPointSize(std::max(10, mono.pointSize()));
    setFont(mono);
    setPlaceholderText("Simulation output will appear here. Input is sent to the running process stdin.");
}

void
ProcessTerminalWidget::attachProcess(QProcess* process)
{
    m_process = process;
    m_historyIndex = -1;
}

void
ProcessTerminalWidget::appendOutput(const QString& text)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
    insertPlainText(text);
    moveCursor(QTextCursor::End);
    m_inputStart = textCursor().position();
    m_promptVisible = false;
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void
ProcessTerminalWidget::resetTerminal()
{
    clear();
    m_inputStart = 0;
    m_history.clear();
    m_historyIndex = -1;
    m_promptVisible = false;
}

void
ProcessTerminalWidget::keyPressEvent(QKeyEvent* event)
{
    ensureCursorInEditableArea();

    if (event->matches(QKeySequence::Copy))
    {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Backspace)
    {
        if (textCursor().position() <= m_inputStart)
        {
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Left)
    {
        if (textCursor().position() <= m_inputStart)
        {
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Home)
    {
        QTextCursor cursor = textCursor();
        cursor.setPosition(m_inputStart);
        setTextCursor(cursor);
        return;
    }

    if (event->key() == Qt::Key_Up)
    {
        if (m_history.isEmpty())
        {
            return;
        }
        if (m_historyIndex < 0)
        {
            m_historyIndex = m_history.size() - 1;
        }
        else
        {
            m_historyIndex = std::max(0, m_historyIndex - 1);
        }
        QTextCursor cursor = textCursor();
        cursor.setPosition(m_inputStart);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(m_history[m_historyIndex]);
        setTextCursor(cursor);
        return;
    }

    if (event->key() == Qt::Key_Down)
    {
        if (m_history.isEmpty())
        {
            return;
        }
        QTextCursor cursor = textCursor();
        cursor.setPosition(m_inputStart);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1)
        {
            ++m_historyIndex;
            cursor.insertText(m_history[m_historyIndex]);
        }
        else
        {
            m_historyIndex = -1;
        }
        setTextCursor(cursor);
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        const QString command = currentInput();
        moveCursor(QTextCursor::End);
        insertPlainText("\n");
        if (!command.trimmed().isEmpty())
        {
            m_history.append(command);
        }
        m_historyIndex = -1;
        if (m_process && m_process->state() != QProcess::NotRunning)
        {
            m_process->write(command.toUtf8());
            m_process->write("\n");
        }
        m_inputStart = textCursor().position();
        m_promptVisible = false;
        return;
    }

    if (event->text().isEmpty() || event->modifiers().testFlag(Qt::ControlModifier))
    {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    showPromptIfNeeded();
    QPlainTextEdit::keyPressEvent(event);
}

void
ProcessTerminalWidget::mousePressEvent(QMouseEvent* event)
{
    QPlainTextEdit::mousePressEvent(event);
    ensureCursorInEditableArea();
}

void
ProcessTerminalWidget::ensureCursorInEditableArea()
{
    QTextCursor cursor = textCursor();
    if (cursor.position() < m_inputStart)
    {
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
    }
}

void
ProcessTerminalWidget::showPromptIfNeeded()
{
    if (m_promptVisible)
    {
        return;
    }
    moveCursor(QTextCursor::End);
    if (!document()->isEmpty() && !toPlainText().endsWith('\n'))
    {
        insertPlainText("\n");
    }
    insertPlainText("> ");
    m_inputStart = textCursor().position();
    m_promptVisible = true;
}

QString
ProcessTerminalWidget::currentInput() const
{
    QTextCursor cursor(document());
    cursor.setPosition(m_inputStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    return cursor.selectedText();
}
