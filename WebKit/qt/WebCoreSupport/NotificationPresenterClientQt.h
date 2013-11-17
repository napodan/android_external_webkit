/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NotificationPresenterClientQt_h
#define NotificationPresenterClientQt_h

#include "Notification.h"
#include "NotificationPresenter.h"

#include <QMultiHash>
#include <QSystemTrayIcon>


#if ENABLE(NOTIFICATIONS)
class QWebPage;

namespace WebCore {
class Document;
class KURL;

struct NotificationIconWrapper {
    NotificationIconWrapper();
    ~NotificationIconWrapper();
#ifndef QT_NO_SYSTEMTRAYICON
    QSystemTrayIcon* m_notificationIcon;
#endif
};

class NotificationPresenterClientQt : public NotificationPresenter {
public:
    NotificationPresenterClientQt(QWebPage*);
    ~NotificationPresenterClientQt() {}

    /* WebCore::NotificationPresenter interface */
    virtual bool show(Notification*);
    virtual void cancel(Notification*);
    virtual void notificationObjectDestroyed(Notification*);
    virtual void requestPermission(SecurityOrigin*, PassRefPtr<VoidCallback>);
    virtual NotificationPresenter::Permission checkPermission(const KURL&);

    void allowNotificationForOrigin(const QString& origin);
    void clearNotificationsList();

    static bool dumpNotification;

    void setReceiver(QObject* receiver) { m_receiver = receiver; }

private:
    void sendEvent(Notification*, const AtomicString& eventName);
    QWebPage* m_page;
    QMultiHash<QString,  QList<RefPtr<VoidCallback> > > m_pendingPermissionRequests;
    QHash <Notification*, NotificationIconWrapper*> m_notifications;
    QObject* m_receiver;
};
}

#endif
#endif

