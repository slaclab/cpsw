 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

/* Generic python helpers */

#include <cpsw_api_user.h>
#include <cpsw_yaml.h>
#include <cpsw_python.h>
#include <Python.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace cpsw_python {

uint64_t
wrap_Path_loadConfigFromYamlFile(Path p, const char *yamlFile,  const char *yaml_dir)
{
	return IPath_loadConfigFromYamlFile(p.get(), yamlFile, yaml_dir);
}

uint64_t
IPath_loadConfigFromYamlFile(IPath *p, const char *yamlFile,  const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
	return p->loadConfigFromYamlFile( yamlFile, yaml_dir );
}

uint64_t
wrap_Path_loadConfigFromYamlString(Path p, const char *yaml,  const char *yaml_dir)
{
	return IPath_loadConfigFromYamlString(p.get(), yaml, yaml_dir);
}

uint64_t
IPath_loadConfigFromYamlString(IPath *p, const char *yaml,  const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
YAML::Node  conf( CYamlFieldFactoryBase::loadPreprocessedYaml( yaml, yaml_dir ) );
	return p->loadConfigFromYaml( conf );
}

uint64_t
wrap_Path_dumpConfigToYamlFile(Path p, const char *filename, const char *templFilename, const char *yaml_dir)
{
	return IPath_dumpConfigToYamlFile(p.get(), filename, templFilename, yaml_dir);
}

uint64_t
IPath_dumpConfigToYamlFile(IPath *p, const char *filename, const char *templFilename, const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
uint64_t    rval;
YAML::Node  conf;

	if ( templFilename ) {
		conf = CYamlFieldFactoryBase::loadPreprocessedYamlFile( templFilename, yaml_dir );
	}

	rval = p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

std::fstream strm( filename, std::fstream::out );
	strm << emit.c_str() << "\n";

	return rval;
}

std::string
wrap_Path_dumpConfigToYamlString(Path p, const char *templFilename, const char *yaml_dir)
{
	return	IPath_dumpConfigToYamlString(p.get(), templFilename, yaml_dir);
}

std::string
IPath_dumpConfigToYamlString(IPath *p, const char *templFilename, const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
YAML::Node  conf;

	if ( templFilename ) {
		conf = CYamlFieldFactoryBase::loadPreprocessedYamlFile( templFilename, yaml_dir );
	}

	p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

std::ostringstream strm;

	strm << emit.c_str() << "\n";

	return strm.str();
}

std::string
wrap_Val_Base_repr(shared_ptr<IVal_Base> obj)
{
	return IVal_Base_repr(obj.get());
}

std::string
IVal_Base_repr(IVal_Base *obj)
{
	return obj->getPath()->toString();
}


};
