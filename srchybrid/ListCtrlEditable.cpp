#include "stdafx.h"
#include "ListCtrlEditable.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#if (_WIN32_WINNT < 0x501)
typedef struct tagNMLVSCROLL
{
	NMHDR   hdr;
	int     dx;
	int     dy;
} NMLVSCROLL, *LPNMLVSCROLL;

#define LVN_BEGINSCROLL          (LVN_FIRST-80)
#define LVN_ENDSCROLL            (LVN_FIRST-81)
#endif


#define MAX_COLS	2

#define LV_EDIT_CTRL_ID		1001

BEGIN_MESSAGE_MAP(CEditableListCtrl, CListCtrl)
	ON_EN_KILLFOCUS(LV_EDIT_CTRL_ID, OnEnKillFocus)
	ON_NOTIFY_REFLECT(LVN_BEGINSCROLL, OnLvnBeginScroll)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_ENDSCROLL, OnLvnEndScroll)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnNmCustomDraw)
	ON_WM_DESTROY()
	ON_WM_HSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_SETFOCUS()
	ON_WM_VSCROLL()
END_MESSAGE_MAP()

void CEditableListCtrl::EditDone()
{
	CommitEdit();
	m_iEditRow = -1;
	m_iEditCol = -1;
	EditHide();
}

void CEditableListCtrl::EditHide()
{
	if (m_pEditWnd) {
		m_pEditWnd->ShowWindow(SW_HIDE);
		m_pEditWnd = NULL;
	}
}

void CEditableListCtrl::VerifyPos()
{
	VerifyScrollPos();
	ResetTopPosition();
	ResetBottomPosition();

	if (m_iCol == 1)
		ShowEditCtrl();
}

CEditableListCtrl::CEditableListCtrl()
	: m_pEditWnd()
	, m_pctrlEdit()
	, m_pctrlComboBox()
	, m_iRow()
	, m_iCol(1)
	, m_iEditRow(-1)
	, m_iEditCol(-1)
{
}

void CEditableListCtrl::ResetTopPosition()
{
	if (m_pEditWnd) {
		CRect rcThis;
		GetWindowRect(rcThis);
		CRect rect;
		if (GetSubItemRect(m_iRow, m_iCol, LVIR_LABEL, rect)) {
			ClientToScreen(rect);
			if (!rcThis.PtInRect(rect.TopLeft()))
				SendMessage(WM_VSCROLL, SB_LINEUP, 0);
		}
	}
}

void CEditableListCtrl::ResetBottomPosition()
{
	if (m_pEditWnd) {
		CRect rcThis;
		GetWindowRect(rcThis);
		CRect rect;
		if (GetSubItemRect(m_iRow, m_iCol, LVIR_LABEL, rect)) {
			ClientToScreen(rect);
			if (!rcThis.PtInRect(rect.BottomRight()))
				SendMessage(WM_VSCROLL, SB_LINEDOWN, 0);
		}
	}
}

void CEditableListCtrl::VerifyScrollPos()
{
	int nTopRowVal = GetTopIndex();
	int nBottomVal = nTopRowVal + GetCountPerPage();

	if (m_iRow < nTopRowVal)
		for (int j = 0; j < -m_iRow + nTopRowVal; ++j)
			SendMessage(WM_VSCROLL, SB_LINEUP, 0);
	else if (m_iRow > nBottomVal)
		for (int j = 0; j < m_iRow - nTopRowVal; ++j)
			SendMessage(WM_VSCROLL, SB_LINEDOWN, 0);
}

BOOL CEditableListCtrl::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB) {
		CWnd *pWnd = NULL;
		CommitEdit();
		m_iEditRow = m_iEditCol = -1;
		if (GetKeyState(VK_SHIFT) & 0x8000) {
			if (--m_iCol < 1) {
				m_iCol = MAX_COLS - 1;
				while (--m_iRow >= 0 && (GetItemData(m_iRow) & 1));

				if (m_iRow < 0) {
					m_iCol = 1;
					m_iRow = 0;

					// cycle to prev sibling control
					pWnd = GetWindow(GW_HWNDPREV);
					ASSERT(pWnd);
					if (pWnd)
						pWnd->SetFocus();
				}
			}
		} else {
			if (++m_iCol >= MAX_COLS) {
				m_iCol = 1;

				while (++m_iRow < GetItemCount() && (GetItemData(m_iRow) & 1));

				ResetBottomPosition();
				if (m_iRow > GetItemCount() - 1) {
					m_iRow = GetItemCount() - 1;

					// cycle to next sibling control
					pWnd = GetNextWindow(GW_HWNDNEXT);
					ASSERT(pWnd);
					if (pWnd)
						pWnd->SetFocus();
				}
			}
		}
		if (!pWnd) {
			VerifyPos();
			return TRUE;
		}
	}
	return CListCtrl::PreTranslateMessage(pMsg);
}

void CEditableListCtrl::OnSetFocus(CWnd *pOldWnd)
{
	CListCtrl::OnSetFocus(pOldWnd);

	int m = GetItemCount();
	if (m > 0) {
		if (m_iRow >= m)
			m_iRow = m - 1;
		while (m_iRow && (GetItemData(m_iRow) & 1))
			--m_iRow;
	}
	while (m_iRow < m && (GetItemData(m_iRow) & 1))
		++m_iRow;

	VerifyPos();
}

void CEditableListCtrl::CommitEdit()
{
	if (m_iEditCol != -1 && m_iEditRow != -1 && m_pEditWnd && m_pEditWnd->IsWindowVisible()) {
		CString strItem;
		if (m_pEditWnd == m_pctrlEdit)
			m_pctrlEdit->GetWindowText(strItem);
		else //m_pctrlComboBox
			m_pctrlComboBox->GetLBText(m_pctrlComboBox->GetCurSel(), strItem);

		SetItemText(m_iEditRow, m_iEditCol, strItem.Trim());
	}
}

void CEditableListCtrl::ShowEditCtrl()
{
	if (m_iRow >= GetItemCount())
		return;

	EditHide();
	if (GetItemData(m_iRow) & 1)
		return;

	CRect rect;
	GetSubItemRect(m_iRow, m_iCol, LVIR_LABEL, rect);
	if (m_pctrlEdit == NULL) {
		m_pctrlEdit = new CEdit;
		m_pctrlEdit->Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, rect, this, LV_EDIT_CTRL_ID);
		m_pctrlEdit->SetFont(GetFont());
		m_pctrlEdit->ShowWindow(SW_SHOW);
	} else
		m_pctrlEdit->SetWindowPos(NULL, rect.left, rect.top, rect.Width(), rect.Height(), SWP_SHOWWINDOW);
	m_pEditWnd = m_pctrlEdit;

	TCHAR szItem[256];
	LVITEM item;
	item.mask = LVIF_TEXT;
	item.iItem = m_iRow;
	item.iSubItem = m_iCol;
	item.cchTextMax = _countof(szItem);
	item.pszText = szItem;
	szItem[GetItem(&item) ? _countof(szItem) - 1 : 0] = _T('\0');

	m_pctrlEdit->SetWindowText(szItem);
	m_pctrlEdit->SetSel(0, -1);
	m_pctrlEdit->SetFocus();

	m_iEditRow = m_iRow;
	m_iEditCol = m_iCol;
}

void CEditableListCtrl::OnEnKillFocus()
{
	EditDone();
}

void CEditableListCtrl::ShowComboBoxCtrl()
{
	EditHide();

	CRect rect;
	GetSubItemRect(m_iRow, m_iCol, LVIR_LABEL, rect);
	rect.bottom += 100;
	if (!m_pctrlComboBox) {
		m_pctrlComboBox = new CComboBox;
		m_pctrlComboBox->Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WS_BORDER | CBS_DROPDOWN | WS_VSCROLL | WS_HSCROLL, rect, this, 1002);
		m_pctrlComboBox->ShowWindow(SW_SHOW);
		m_pctrlComboBox->SetHorizontalExtent(800);
		m_pctrlComboBox->SendMessage(CB_SETDROPPEDWIDTH, 600, 0);
	} else {
		m_pctrlComboBox->SetWindowPos(NULL, rect.left, rect.top, rect.Width(), rect.Height(), SWP_SHOWWINDOW);
		m_pctrlComboBox->SetCurSel(0);
	}
	m_pEditWnd = m_pctrlComboBox;
	m_pctrlComboBox->SetFocus();

}

void CEditableListCtrl::OnLvnColumnClick(LPNMHDR, LRESULT *pResult)
{
	EditHide();
	*pResult = 0;
}

void CEditableListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CListCtrl::OnLButtonDown(nFlags, point);

	LVHITTESTINFO hti;
	hti.pt = point;
	if (SubItemHitTest(&hti) <= 0)
		return;

	int iItem = hti.iItem;
	int iSubItem = hti.iSubItem;
	if (iItem <= 0 || iSubItem <= 0)
		return;

	CRect rect;
	GetSubItemRect(iItem, iSubItem, LVIR_LABEL, rect);
	ClientToScreen(rect);

	CRect rc;
	GetWindowRect(rc);
	if (!rc.PtInRect(rect.BottomRight()))
		SendMessage(WM_VSCROLL, SB_LINEDOWN, 0);
	if (!rc.PtInRect(rect.TopLeft()))
		SendMessage(WM_VSCROLL, SB_LINEUP, 0);

	m_iRow = iItem;
	m_iCol = iSubItem;

	switch (m_iCol) {
	case 0:
		m_iCol = 1; //move to column 1
	case 1:
		ShowEditCtrl(); //start editing
	}
}

void CEditableListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	if (nSBCode != SB_LINEDOWN && nSBCode != SB_LINEUP) {
		if (nSBCode == SB_ENDSCROLL) {
			if (m_pctrlEdit)
				ShowEditCtrl();
		} else
			EditDone();
	}

	CListCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CEditableListCtrl::OnLvnBeginScroll(LPNMHDR, LRESULT *pResult)
{
	//LPNMLVSCROLL pStateChanged = reinterpret_cast<LPNMLVSCROLL>(pNMHDR);
	EditDone();
	*pResult = 0;
}

void CEditableListCtrl::OnLvnEndScroll(LPNMHDR, LRESULT *pResult)
{
	//LPNMLVSCROLL pStateChanged = reinterpret_cast<LPNMLVSCROLL>(pNMHDR);
	if (m_pctrlEdit)
		ShowEditCtrl();
	*pResult = 0;
}

void CEditableListCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	EditHide();
	CListCtrl::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CEditableListCtrl::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
	switch (reinterpret_cast<LPNMHDR>(lParam)->code) {
	case HDN_BEGINTRACKW:
	case HDN_BEGINTRACKA:
		EditHide();
		*pResult = 0;
	}

	return CListCtrl::OnNotify(wParam, lParam, pResult);
}

void CEditableListCtrl::OnDestroy()
{
	delete m_pctrlEdit;
	delete m_pctrlComboBox;
	CListCtrl::OnDestroy();
}

void CEditableListCtrl::OnNmCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVCUSTOMDRAW pNMCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);

	if (pNMCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}

	if (pNMCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
		pNMCD->clrText = (pNMCD->nmcd.lItemlParam & 1) ? RGB(128, 128, 128) : CLR_DEFAULT; //Bit 0 = disabled
		pNMCD->clrTextBk = CLR_DEFAULT;
	}

	*pResult = CDRF_DODEFAULT;
}