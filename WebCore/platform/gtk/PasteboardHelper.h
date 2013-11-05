/*
 * Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2010 Martin Robinson <mrobinson@webkit.org>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PasteboardHelper_h
#define PasteboardHelper_h

/*
 * FIXME: this is for WebCore support and must be removed once
 * a better solution is found
 */

#include "Frame.h"

typedef struct _GtkClipboard GtkClipboard;
typedef struct _GtkTargetList GtkTargetList;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkSelectionData GtkSelectionData;

namespace WebCore {

class DataObjectGtk;

class PasteboardHelper {
public:
    PasteboardHelper();
    virtual ~PasteboardHelper();

    GtkClipboard* getCurrentClipboard(Frame*);
    GtkClipboard* getClipboard(Frame*) const;
    GtkClipboard* getPrimarySelectionClipboard(Frame*) const;
    GtkTargetList* targetList() const;
    void fillSelectionData(GtkSelectionData*, guint, DataObjectGtk*);
    void writeClipboardContents(GtkClipboard*, GClosure*);

    enum PasteboardTargetType { TargetTypeText, TargetTypeMarkup, TargetTypeURIList, TargetTypeNetscapeURL, TargetTypeImage, TargetTypeUnknown };
    virtual guint getIdForTargetType(PasteboardTargetType) = 0;

protected:
    void initializeTargetList();
    virtual bool usePrimarySelectionClipboard(GtkWidget*) = 0;

private:
    GtkTargetList* m_targetList;
    GtkTargetList* targetListForDataObject(DataObjectGtk*);
};

}

#endif // PasteboardHelper_h
