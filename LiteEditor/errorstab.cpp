//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : buidltab.cpp
//
// -------------------------------------------------------------------------
// A
//              _____           _      _     _ _
//             /  __ \         | |    | |   (_) |
//             | /  \/ ___   __| | ___| |    _| |_ ___
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/
//              \____/\___/ \__,_|\___\_____/_|\__\___|
//
//                                                  F i l e
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
#include "globals.h"
#include <wx/xrc/xmlres.h>
#include "errorstab.h"
#include "findresultstab.h"


#ifndef wxScintillaEventHandler
#define wxScintillaEventHandler(func) \
	(wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxScintillaEventFunction, &func)
#endif

ErrorsTab::ErrorsTab(BuildTab *bt, wxWindow *parent, wxWindowID id, const wxString &name)
    : OutputTabWindow(parent, id, name)
    , m_bt(bt)
{
    m_tb->DeleteTool(XRCID("clear_all_output"));
    m_tb->AddSeparator();

    m_tb->AddCheckTool(XRCID("show_errors"), wxT("Errors"), wxXmlResource::Get()->LoadBitmap(wxT("project_conflict")), wxNullBitmap, wxT("Show build errors"));
    m_tb->ToggleTool(XRCID("show_errors"), true);
    m_tb->Connect(XRCID("show_errors"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ErrorsTab::OnRedisplayLines), NULL, this);

    m_tb->AddCheckTool(XRCID("show_warnings"), wxT("Warnings"), wxXmlResource::Get()->LoadBitmap(wxT("help_icon")), wxNullBitmap, wxT("Show build warnings"));
    m_tb->ToggleTool(XRCID("show_warnings"), true);
    m_tb->Connect(XRCID("show_warnings"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ErrorsTab::OnRedisplayLines), NULL, this);

    m_tb->AddCheckTool(XRCID("show_build_lines"), wxT("Build"), wxXmlResource::Get()->LoadBitmap(wxT("todo")), wxNullBitmap, wxT("Show build status lines"));
    m_tb->ToggleTool(XRCID("show_build_lines"), false);
    m_tb->Connect(XRCID("show_build_lines"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ErrorsTab::OnRedisplayLines), NULL, this);
	m_tb->Realize();
    
    FindResultsTab::SetStyles(m_sci);
    
 	m_sci->IndicatorSetForeground(1, MakeColourLighter(wxT("GOLD"), 5));
 	m_sci->IndicatorSetForeground(2, MakeColourLighter(wxT("RED"), 4));
	m_sci->IndicatorSetStyle(1, wxSCI_INDIC_ROUNDBOX);
	m_sci->IndicatorSetStyle(2, wxSCI_INDIC_ROUNDBOX);
	m_sci->IndicatorSetUnder(1, true);
	m_sci->IndicatorSetUnder(2, true);
   
    // current line marker
	m_sci->SetMarginWidth(1, 0);
   	m_sci->MarkerDefine(0x7, wxSCI_MARK_ARROW);
	m_sci->MarkerSetBackground(0x7, wxT("PINK"));
	m_sci->MarkerSetForeground(0x7, wxT("BLACK"));
    
	Connect(wxEVT_SCI_MARGINCLICK, wxScintillaEventHandler(ErrorsTab::OnMarginClick), NULL, this);
}

ErrorsTab::~ErrorsTab()
{
}

void ErrorsTab::ClearLines()
{
    Clear();
    m_lineMap.clear();
}

bool ErrorsTab::IsShowing(int linecolor)
{
    switch (linecolor) {
        case wxSCI_LEX_GCC_BUILDING:
            return m_tb->GetToolState(XRCID("show_build_lines"));
        case wxSCI_LEX_GCC_ERROR:
            return m_tb->GetToolState(XRCID("show_errors"));
        case wxSCI_LEX_GCC_WARNING:
            return m_tb->GetToolState(XRCID("show_warnings"));
    }
    return false;
}

void ErrorsTab::AppendLine(int line)
{
    std::map<int,BuildTab::LineInfo>::iterator i = m_bt->m_lineInfo.find(line);
    if (i == m_bt->m_lineInfo.end() || !IsShowing(i->second.linecolor))
        return;
    if (i->second.linecolor == wxSCI_LEX_GCC_BUILDING) {
        if (i->second.linetext[0] == wxT('-')) {
            m_lineMap[m_sci->GetLineCount()-1] = line;
            AppendText(i->second.linetext);
        }
        return;
    }
    
    wxString filename = i->second.linetext.Mid(i->second.filestart, i->second.filelen);
    wxString prevfile;
    if (!m_lineMap.empty()) {
        std::map<int,BuildTab::LineInfo>::iterator p = m_bt->m_lineInfo.find(m_lineMap.rbegin()->second);
        prevfile = p->second.linetext.Mid(p->second.filestart, p->second.filelen);
    }
    if (prevfile != filename) {
        // new file -- put file name on its own line
        AppendText(filename + wxT("\n"));
    }
    
    int lineno = m_sci->GetLineCount()-1;
    m_lineMap[lineno] = line;
    
    // remove "...filename:" from line text
    wxString text = i->second.linetext.Mid(i->second.filestart + i->second.filelen);
    if (!text.IsEmpty() && text[0] == wxT(':')) {
        text = text.Mid(1);
    }
    // pad (possible) line number to 5 spaces
    int pos = text.Find(wxT(':'));
    if (pos < 0 || pos > 4) {
        pos = 0;
    }
    text.Pad(5-pos, wxT(' '), false);
    
    AppendText(text);
    m_sci->SetIndicatorCurrent(i->second.linecolor == wxSCI_LEX_GCC_ERROR ? 2 : 1);
    m_sci->IndicatorFillRange(m_sci->PositionFromLine(lineno), 5);
}

void ErrorsTab::MarkLine(int line)
{
    std::map<int,BuildTab::LineInfo>::iterator i = m_bt->m_lineInfo.find(line);
    if (i == m_bt->m_lineInfo.end() || !IsShowing(i->second.linecolor))
        return;
    for (std::map<int,int>::iterator j = m_lineMap.begin(); j != m_lineMap.end(); j++) {
        if (j->second == line) {
            m_sci->MarkerDeleteAll(0x7);
            m_sci->MarkerAdd(j->first, 0x7);
            m_sci->SetCurrentPos(m_sci->PositionFromLine(j->first));
            m_sci->SetSelection(-1, m_sci->GetCurrentPos());
            m_sci->EnsureCaretVisible();
        }
    }
}

void ErrorsTab::OnRedisplayLines(wxCommandEvent& e)
{
    wxUnusedVar(e);

    int marked = -1;
    ClearLines();
    for (int i = 0; i < m_bt->m_sci->GetLineCount(); i++) {
        AppendLine(i);
        if (m_bt->m_sci->MarkerGet(i) & 1<<0x7) {
            marked = i;
        }
    }
    if (marked >= 0) {
        MarkLine(marked);
    }
}

void ErrorsTab::OnHotspotClicked(wxScintillaEvent &e)
{
	OnMouseDClick(e);
}

void ErrorsTab::OnMouseDClick(wxScintillaEvent &e)
{
    m_sci->SetSelection(-1, m_sci->GetCurrentPos());
    std::map<int,int>::iterator i = m_lineMap.find(m_sci->LineFromPosition(e.GetPosition()));
    if (i != m_lineMap.end()) {
        std::map<int,BuildTab::LineInfo>::iterator m = m_bt->m_lineInfo.find(i->second);
        if (m != m_bt->m_lineInfo.end() || m->second.linecolor != wxSCI_LEX_GCC_BUILDING) {
            m_bt->DoMarkAndOpenFile(m, true);
            return;
        }
    }
    m_sci->ToggleFold(m_sci->LineFromPosition(e.GetPosition()));
}

void ErrorsTab::OnMarginClick(wxScintillaEvent& e)
{
	if (e.GetMargin() == 4) {
		m_sci->ToggleFold(m_sci->LineFromPosition(e.GetPosition()));
	}
}
