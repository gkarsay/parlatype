# -*- coding: utf-8 -*-
'''
Parlatype.py is part of Parlatype.
Version: 1.6.1

Copyright (C) Gabor Karsay 2016-2019 <gabor.karsay@gmx.at>

This program is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <https://www.gnu.org/licenses/>.
'''

import dbus
import msgbox


def _showMessage(message):
    myBox = msgbox.MsgBox(XSCRIPTCONTEXT.getComponentContext())
    myBox.addButton("OK")
    myBox.renderFromButtonSize()
    myBox.numberOflines = 2
    myBox.show(message, 0, "Parlatype for LibreOffice")


def _getDBUSService():
    try:
        obj = dbus.SessionBus().get_object(
            "com.github.gkarsay.parlatype",
            "/com/github/gkarsay/parlatype")
    except Exception:
        # this seems to succeed always, exception never triggered
        return None
    return dbus.Interface(obj, "com.github.gkarsay.parlatype")


def _getTextRange():
    doc = XSCRIPTCONTEXT.getDocument()

    # the writer controller impl supports
    # the css.view.XSelectionSupplier interface
    xSelectionSupplier = doc.getCurrentController()

    xIndexAccess = xSelectionSupplier.getSelection()
    count = xIndexAccess.getCount()

    # don't mess around with multiple selections
    if (count != 1):
        return None

    textrange = xIndexAccess.getByIndex(0)

    # don't mess around with selections, just plain cursor
    if (len(textrange.getString()) == 0):
        return textrange
    else:
        return None


def InsertTimestamp(*args):
    textrange = _getTextRange()
    if (textrange is None):
        return

    service = _getDBUSService()

    try:
        textrange.setString(service.GetTimestamp())
    except Exception:
        pass


def InsertTimestampOnNewLine(*args):
    textrange = _getTextRange()
    if (textrange is None):
        return

    service = _getDBUSService()

    try:
        textrange.setString("\n" + service.GetTimestamp() + " ")
    except Exception:
        pass


def _isValidCharacter(char):
    if (char.isdigit() or char == ":" or char == "." or char == "-"):
        return True
    return False


def _extractTimestamp():
    textrange = _getTextRange()
    if (textrange is None):
        return None

    xText = textrange.getText()
    cursor = xText.createTextCursorByRange(textrange)

    # select first char on the left, no success if at start of document
    success = cursor.goLeft(1, True)

    if (success):
        i = 0
        while (_isValidCharacter(cursor.getString()[0]) and i < 15):
            success = cursor.goLeft(1, True)
            i += 1
        if (success):
            cursor.goRight(1, True)
        cursor.collapseToStart()

    cursor.goRight(2, True)

    i = 0
    while (_isValidCharacter(cursor.getString()[-1:]) and i < 15):
        success = cursor.goRight(1, True)
        i += 1
    if (success):
        cursor.goLeft(1, True)

    return cursor.getString()


def GotoTimestamp(*args):
    timestamp = _extractTimestamp()
    if (timestamp is None):
        return

    service = _getDBUSService()

    try:
        service.GotoTimestamp(timestamp)
    except Exception:
        pass


def PlayPause(*args):
    service = _getDBUSService()

    try:
        service.PlayPause()
    except Exception:
        pass


def JumpBack(*args):
    service = _getDBUSService()

    try:
        service.JumpBack()
    except Exception:
        pass


def JumpForward(*args):
    service = _getDBUSService()

    try:
        service.JumpForward()
    except Exception:
        pass


def IncreaseSpeed(*args):
    service = _getDBUSService()

    try:
        service.IncreaseSpeed()
    except Exception:
        pass


def DecreaseSpeed(*args):
    service = _getDBUSService()

    try:
        service.DecreaseSpeed()
    except Exception:
        pass


# Lists the scripts, that shall be visible inside LibreOffice.
g_exportedScripts = \
    InsertTimestamp,\
    InsertTimestampOnNewLine,\
    GotoTimestamp,\
    PlayPause,\
    JumpBack,\
    JumpForward,\
    IncreaseSpeed,\
    DecreaseSpeed,
