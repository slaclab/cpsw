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

	typedef CGetValWrapperContextTmpl<PyUniqueObj, PyListObj> CGetValWrapperContext;

	class CPyBase {
	protected:
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

	class CAsyncIO : public IAsyncIO, public CPyBase, public CGetValWrapperContext {
	public:
		CAsyncIO(PyObject *pyObj);
		CAsyncIO();

		virtual void init(PyObject *);

		virtual void callback(CPSWError *status);
		virtual void callback(PyObject *me, PyObject *status);

		// Each shared_ptr<CAsyncIO> takes out a python reference
		// which is dropped when the shared_ptr<CAsyncIO> is deleted,
		// i.e., each shared_ptr control block (of which there could
		// be multiple ones -- unlike regular use of shared_ptr!)
		// 'owns' one python reference.
		// This object is meant to be embedded into a PyObject!
		virtual shared_ptr<CAsyncIO> makeShared();
	};

};

#include <pycpsw.h>

#endif
