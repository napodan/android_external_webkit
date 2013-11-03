/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
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
 */

#include "config.h"
#include "HTMLLinkElement.h"

#include "CSSHelper.h"
#include "CachedCSSStyleSheet.h"
#include "DNS.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "ScriptEventListener.h"
#include "Settings.h"
#include <wtf/StdLibExtras.h>
#if ENABLE(LINK_PREFETCH)
#include "CachedLinkPrefetch.h"
#endif

namespace WebCore {

using namespace HTMLNames;

HTMLLinkElement::HTMLLinkElement(const QualifiedName& qName, Document *doc, bool createdByParser)
    : HTMLElement(qName, doc)
    , m_cachedSheet(0)
    , m_disabledState(0)
    , m_loading(false)
    , m_createdByParser(createdByParser)
    , m_timer(this, &HTMLLinkElement::timerFired)
{
    ASSERT(hasTagName(linkTag));
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_cachedSheet) {
        m_cachedSheet->removeClient(this);
        if (m_loading && !isDisabled() && !isAlternate())
            document()->removePendingSheet();
    }
}

void HTMLLinkElement::setDisabledState(bool _disabled)
{
    int oldDisabledState = m_disabledState;
    m_disabledState = _disabled ? 2 : 1;
    if (oldDisabledState != m_disabledState) {
        // If we change the disabled state while the sheet is still loading, then we have to
        // perform three checks:
        if (isLoading()) {
            // Check #1: If the sheet becomes disabled while it was loading, and if it was either
            // a main sheet or a sheet that was previously enabled via script, then we need
            // to remove it from the list of pending sheets.
            if (m_disabledState == 2 && (!m_rel.m_isAlternate || oldDisabledState == 1))
                document()->removePendingSheet();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_rel.m_isAlternate && m_disabledState == 1)
                document()->addPendingSheet();

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script.  It takes really terrible code to make this
            // happen (a double toggle for no reason essentially).  This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_rel.m_isAlternate && m_disabledState == 1 && oldDisabledState == 2)
                document()->addPendingSheet();

            // If the sheet is already loading just bail.
            return;
        }

        // Load the sheet, since it's never been loaded before.
        if (!m_sheet && m_disabledState == 1)
            process();
        else
            document()->updateStyleSelector(); // Update the style selector.
    }
}

StyleSheet* HTMLLinkElement::sheet() const
{
    return m_sheet.get();
}

void HTMLLinkElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == relAttr) {
        tokenizeRelAttribute(attr->value(), m_rel);
        process();
    } else if (attr->name() == hrefAttr) {
        m_url = document()->completeURL(deprecatedParseURL(attr->value()));
        process();
    } else if (attr->name() == typeAttr) {
        m_type = attr->value();
        process();
    } else if (attr->name() == mediaAttr) {
        m_media = attr->value().string().lower();
        process();
    } else if (attr->name() == disabledAttr) {
        setDisabledState(!attr->isNull());
    } else if (attr->name() == onbeforeloadAttr) {
        setAttributeEventListener(eventNames().beforeloadEvent, createAttributeEventListener(this, attr));
    } else if (attr->name() == onloadAttr) {
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attr));
    } else {
        if (attr->name() == titleAttr && m_sheet)
            m_sheet->setTitle(attr->value());
        HTMLElement::parseMappedAttribute(attr);
    }
}

void HTMLLinkElement::tokenizeRelAttribute(const AtomicString& rel, RelAttribute& attribute)
{
    attribute.m_isStyleSheet = false;
    attribute.m_isIcon = false;
    attribute.m_isAlternate = false;
    attribute.m_isDNSPrefetch = false;
#if ENABLE(LINK_PREFETCH)
    attribute.m_isLinkPrefetch = false;
#endif
#ifdef ANDROID_APPLE_TOUCH_ICON
    attribute.m_isTouchIcon = false;
    attribute.m_isPrecomposedTouchIcon = false;
#endif
    if (equalIgnoringCase(rel, "stylesheet"))
        attribute.m_isStyleSheet = true;
    else if (equalIgnoringCase(rel, "icon") || equalIgnoringCase(rel, "shortcut icon"))
        attribute.m_isIcon = true;
#ifdef ANDROID_APPLE_TOUCH_ICON
    else if (equalIgnoringCase(rel, "apple-touch-icon"))
        attribute.m_isTouchIcon = true;
    else if (equalIgnoringCase(rel, "apple-touch-icon-precomposed"))
        attribute.m_isPrecomposedTouchIcon = true;
#endif
    else if (equalIgnoringCase(rel, "dns-prefetch"))
        attribute.m_isDNSPrefetch = true;
#if ENABLE(LINK_PREFETCH)
    else if (equalIgnoringCase(rel, "prefetch"))
        attribute.m_isLinkPrefetch = true;
#endif
    else if (equalIgnoringCase(rel, "alternate stylesheet") || equalIgnoringCase(rel, "stylesheet alternate")) {
        attribute.m_isStyleSheet = true;
        attribute.m_isAlternate = true;
    } else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        String relString = rel.string();
        relString.replace('\n', ' ');
        Vector<String> list;
        relString.split(' ', list);
        Vector<String>::const_iterator end = list.end();
        for (Vector<String>::const_iterator it = list.begin(); it != end; ++it) {
            if (equalIgnoringCase(*it, "stylesheet"))
                attribute.m_isStyleSheet = true;
            else if (equalIgnoringCase(*it, "alternate"))
                attribute.m_isAlternate = true;
            else if (equalIgnoringCase(*it, "icon"))
                attribute.m_isIcon = true;
        }
    }
}

void HTMLLinkElement::process()
{
    if (!inDocument())
        return;

    String type = m_type.lower();

    // IE extension: location of small icon for locationbar / bookmarks
    // We'll record this URL per document, even if we later only use it in top level frames
    if (m_rel.m_isIcon && m_url.isValid() && !m_url.isEmpty())
        document()->setIconURL(m_url.string(), type);

#ifdef ANDROID_APPLE_TOUCH_ICON
    if ((m_rel.m_isTouchIcon || m_rel.m_isPrecomposedTouchIcon) && m_url.isValid()
            && !m_url.isEmpty())
        document()->frame()->loader()->client()
                ->dispatchDidReceiveTouchIconURL(m_url.string(),
                        m_rel.m_isPrecomposedTouchIcon);
#endif

    if (m_rel.m_isDNSPrefetch && document()->isDNSPrefetchEnabled() && m_url.isValid() && !m_url.isEmpty())
        prefetchDNS(m_url.host());

#if ENABLE(LINK_PREFETCH)
    if (m_rel.m_isLinkPrefetch && m_url.isValid()) {
        m_cachedLinkPrefetch = document()->docLoader()->requestLinkPrefetch(m_url);
        m_loading = true;

        if (m_cachedLinkPrefetch)
            m_cachedLinkPrefetch->addClient(this);
    }
#endif

    bool acceptIfTypeContainsTextCSS = document()->page() && document()->page()->settings() && document()->page()->settings()->treatsAnyTextCSSLinkAsStylesheet();

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if (m_disabledState != 2 && (m_rel.m_isStyleSheet || (acceptIfTypeContainsTextCSS && type.contains("text/css"))) && document()->frame() && m_url.isValid()) {
        // also, don't load style sheets for standalone documents
        
        String charset = getAttribute(charsetAttr);
        if (charset.isEmpty() && document()->frame())
            charset = document()->frame()->loader()->encoding();

        if (m_cachedSheet) {
            if (m_loading)
                document()->removePendingSheet();
            m_cachedSheet->removeClient(this);
            m_cachedSheet = 0;
        }

        if (!dispatchBeforeLoadEvent(m_url))
            return;
        
        m_loading = true;
        
        // Add ourselves as a pending sheet, but only if we aren't an alternate 
        // stylesheet.  Alternate stylesheets don't hold up render tree construction.
        if (!isAlternate())
            document()->addPendingSheet();

        m_cachedSheet = document()->docLoader()->requestCSSStyleSheet(m_url, charset);
        
        if (m_cachedSheet)
            m_cachedSheet->addClient(this);
        else {
            // The request may have been denied if (for example) the stylesheet is local and the document is remote.
            m_loading = false;
            if (!isAlternate())
                document()->removePendingSheet();
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        m_sheet = 0;
        document()->updateStyleSelector();
    }
}

void HTMLLinkElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    document()->addStyleSheetCandidateNode(this, m_createdByParser);
    process();
}

void HTMLLinkElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();

    document()->removeStyleSheetCandidateNode(this);

    // FIXME: It's terrible to do a synchronous update of the style selector just because a <style> or <link> element got removed.
    if (document()->renderer())
        document()->updateStyleSelector();
}

void HTMLLinkElement::finishParsingChildren()
{
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLLinkElement::setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* sheet)
{
    m_sheet = CSSStyleSheet::create(this, href, baseURL, charset);

    bool strictParsing = !document()->inCompatMode();
    bool enforceMIMEType = strictParsing;
    bool crossOriginCSS = false;
    bool validMIMEType = false;
    bool needsSiteSpecificQuirks = document()->page() && document()->page()->settings()->needsSiteSpecificQuirks();

    // Check to see if we should enforce the MIME type of the CSS resource in strict mode.
    // Running in iWeb 2 is one example of where we don't want to - <rdar://problem/6099748>
    if (enforceMIMEType && document()->page() && !document()->page()->settings()->enforceCSSMIMETypeInStrictMode())
        enforceMIMEType = false;

#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
    if (enforceMIMEType && needsSiteSpecificQuirks) {
        // Covers both http and https, with or without "www."
        if (baseURL.string().contains("mcafee.com/japan/", false))
            enforceMIMEType = false;
    }
#endif

    String sheetText = sheet->sheetText(enforceMIMEType, &validMIMEType);
    m_sheet->parseString(sheetText, strictParsing);

    // If we're loading a stylesheet cross-origin, and the MIME type is not
    // standard, require the CSS to at least start with a syntactically
    // valid CSS rule.
    // This prevents an attacker playing games by injecting CSS strings into
    // HTML, XML, JSON, etc. etc.
    if (!document()->securityOrigin()->canRequest(baseURL))
        crossOriginCSS = true;

    if (crossOriginCSS && !validMIMEType && !m_sheet->hasSyntacticallyValidCSSHeader())
        m_sheet = CSSStyleSheet::create(this, href, baseURL, charset);

    if (strictParsing && needsSiteSpecificQuirks) {
        // Work around <https://bugs.webkit.org/show_bug.cgi?id=28350>.
        DEFINE_STATIC_LOCAL(const String, slashKHTMLFixesDotCss, ("/KHTMLFixes.css"));
        DEFINE_STATIC_LOCAL(const String, mediaWikiKHTMLFixesStyleSheet, ("/* KHTML fix stylesheet */\n/* work around the horizontal scrollbars */\n#column-content { margin-left: 0; }\n\n"));
        // There are two variants of KHTMLFixes.css. One is equal to mediaWikiKHTMLFixesStyleSheet,
        // while the other lacks the second trailing newline.
        if (baseURL.string().endsWith(slashKHTMLFixesDotCss) && !sheetText.isNull() && mediaWikiKHTMLFixesStyleSheet.startsWith(sheetText)
                && sheetText.length() >= mediaWikiKHTMLFixesStyleSheet.length() - 1) {
            ASSERT(m_sheet->length() == 1);
            ExceptionCode ec;
            m_sheet->deleteRule(0, ec);
        }
    }

    m_sheet->setTitle(title());

    RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(m_media);
    m_sheet->setMedia(media.get());

    m_loading = false;
    m_sheet->checkLoaded();
}

bool HTMLLinkElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

void HTMLLinkElement::notifyFinished(CachedResource*)
{
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void HTMLLinkElement::timerFired(Timer<HTMLLinkElement>*)
{
    dispatchEvent(Event::create(eventNames().loadEvent, false, false));
}

bool HTMLLinkElement::sheetLoaded()
{
    if (!isLoading() && !isDisabled() && !isAlternate()) {
        document()->removePendingSheet();
        return true;
    }
    return false;
}

bool HTMLLinkElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == hrefAttr;
}

bool HTMLLinkElement::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLLinkElement::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

String HTMLLinkElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLLinkElement::setCharset(const String& value)
{
    setAttribute(charsetAttr, value);
}

KURL HTMLLinkElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

void HTMLLinkElement::setHref(const String& value)
{
    setAttribute(hrefAttr, value);
}

String HTMLLinkElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLLinkElement::setHreflang(const String& value)
{
    setAttribute(hreflangAttr, value);
}

String HTMLLinkElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLLinkElement::setMedia(const String& value)
{
    setAttribute(mediaAttr, value);
}

String HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLLinkElement::setRel(const String& value)
{
    setAttribute(relAttr, value);
}

String HTMLLinkElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLLinkElement::setRev(const String& value)
{
    setAttribute(revAttr, value);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLLinkElement::setTarget(const String& value)
{
    setAttribute(targetAttr, value);
}

String HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLinkElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

void HTMLLinkElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    // Favicons are handled by a special case in LegacyWebArchive::create()
    if (m_rel.m_isIcon)
        return;

    if (!m_rel.m_isStyleSheet)
        return;
    
    // Append the URL of this link element.
    addSubresourceURL(urls, href());
    
    // Walk the URLs linked by the linked-to stylesheet.
    if (StyleSheet* styleSheet = const_cast<HTMLLinkElement*>(this)->sheet())
        styleSheet->addSubresourceStyleURLs(urls);
}

#ifdef ANDROID_INSTRUMENT
void* HTMLLinkElement::operator new(size_t size)
{
    return Node::operator new(size);
}

void* HTMLLinkElement::operator new[](size_t size)
{
    return Node::operator new[](size);
}

void HTMLLinkElement::operator delete(void* p, size_t size)
{
    Node::operator delete(p, size);
}

void HTMLLinkElement::operator delete[](void* p, size_t size)
{
    Node::operator delete[](p, size);
}
#endif

}
