 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_CYTHON_H
#define CPSW_CYTHON_H

#include <cpsw_python.h>
#include <cpsw_api_user.h>

namespace cpsw_python {

	typedef const IEntry CIEntry;
	typedef const IChild CIChild;
	typedef const IHub   CIHub;
	typedef const IPath  CIPath;

	typedef Entry               cc_Entry;
	typedef Child               cc_Child;
	typedef Children            cc_Children;
	typedef shared_ptr<CIHub>   cc_Hub;
	typedef Path                cc_Path;
	typedef ConstPath           cc_ConstPath;
	typedef CIEntry            *CIEntryp;
	typedef CIChild            *CIChildp;
	typedef CIHub              *CIHubp;
	typedef CIPath             *CIPathp;
	typedef IPath              *IPathp;

	class CPyBase {
	private:
		PyObject *me_;
	public:
		CPyBase()
		: me_( NULL )
		{
		}

		CPyBase(PyObject *me)
		: me_( me )
		{
		}

		PyObject *me()
		{
			return me_;
		}
	};

	// Have to go through these callback hoops because
	// in cython we can't apparently create members
	// that don't have a PyObject* first arg.

	class CPathVisitor : public IPathVisitor, public CPyBase {
	public:
		CPathVisitor()
		{
		}

		CPathVisitor(PyObject *me)
		: CPyBase( me )
		{
		}

		bool visitPre(ConstPath here);

		bool visitPre(PyObject *me, ConstPath here);

		void visitPost(ConstPath here);
		void visitPost(PyObject *me, ConstPath here);
    };

	class CYamlFixup : public IYamlFixup, public CPyBase {
	public:
		CYamlFixup()
		{
		}

		CYamlFixup(PyObject *me)
		: CPyBase( me )
		{
		}

		virtual void operator()(YAML::Node &root, YAML::Node &top);

		virtual void call(PyObject *obj, YAML::Node &root, YAML::Node &top);

	};

	PyObject *
	IScalVal_RO_getVal(IScalVal_RO *val, int fromIdx, int toIdx, bool forceNumeric);
};

#include <pycpsw.h>

#endif
