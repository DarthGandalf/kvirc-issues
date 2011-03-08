#ifndef _KVI_KVS_TREENODE_CONSTANTDATA_H_
#define _KVI_KVS_TREENODE_CONSTANTDATA_H_
//=============================================================================
//
//   File : kvi_kvs_treenode_constantdata.h
//   Creation date : Tue 07 Oct 2003 02:21:59 by Szymon Stefanek
//
//   This file is part of the KVIrc IRC client distribution
//   Copyright (C) 2003-2008 Szymon Stefanek <pragma at kvirc dot net>
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
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

#include "kvi_settings.h"
#include "kvi_qstring.h"
#include "kvi_kvs_treenode_data.h"

class KVIRC_API KviKvsTreeNodeConstantData : public KviKvsTreeNodeData
{
public:
	KviKvsTreeNodeConstantData(const QChar * pLocation,KviKvsVariant * v);
	~KviKvsTreeNodeConstantData();
protected:
	KviKvsVariant * m_pValue; // literal value of the parameter
public:
	virtual bool evaluateReadOnly(KviKvsRunTimeContext * c,KviKvsVariant *pBuffer);
	virtual void contextDescription(QString &szBuffer);

	virtual void dump(const char * prefix);

	virtual bool convertStringConstantToNumeric();

	KviKvsVariant * value(){ return m_pValue; };
};


#endif //!_KVI_KVS_TREENODE_CONSTANTDATA_H_