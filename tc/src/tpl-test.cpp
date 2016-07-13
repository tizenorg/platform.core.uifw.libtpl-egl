/**
 * @file tpl-test.cpp
 * @brief TPLTestBase class functions are defined in this file
 */ 

#include "tpl-test.h"


TPLBackendBase *TPLTestBase::backend = (TPLBackendBase *)NULL;
Config TPLTestBase::config;


void
TPLTestBase::SetUpTestCase()
{
	backend->tpl_backend_initialize(&config);
}


void
TPLTestBase::TearDownTestCase()
{
	backend->tpl_backend_finalize(&config);
}

