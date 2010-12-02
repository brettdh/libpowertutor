#include <cppunit/Exception.h>
#include <cppunit/SourceLine.h>
#include <cppunit/TestFailure.h>
#include <cstdio>
#include "StdioOutputter.h"
#include <cppunit/TestResultCollector.h>
#include <cutils/log.h>

using namespace CppUnit;

StdioOutputter::StdioOutputter( TestResultCollector *result )
    : m_result( result )
{
}


StdioOutputter::~StdioOutputter()
{
}


void 
StdioOutputter::write() 
{
    printHeader();
    LOGD("\n");
    printFailures();
    LOGD("\n");
}


void 
StdioOutputter::printFailures()
{
    TestResultCollector::TestFailures::const_iterator itFailure = m_result->failures().begin();
    int failureNumber = 1;
    while ( itFailure != m_result->failures().end() ) 
    {
        LOGD("\n");
        printFailure( *itFailure++, failureNumber++ );
    }
}


void 
StdioOutputter::printFailure( TestFailure *failure,
                             int failureNumber )
{
    printFailureListMark( failureNumber );
    LOGD(" ");
    printFailureTestName( failure );
    LOGD(" ");
    printFailureType( failure );
    LOGD(" ");
    printFailureLocation( failure->sourceLine() );
    LOGD("\n");
    printFailureDetail( failure->thrownException() );
    LOGD("\n");
}


void 
StdioOutputter::printFailureListMark( int failureNumber )
{
    LOGD("%d)",failureNumber);
}


void 
StdioOutputter::printFailureTestName( TestFailure *failure )
{
    LOGD("test: %s", failure->failedTestName().c_str());
}


void 
StdioOutputter::printFailureType( TestFailure *failure )
{
    LOGD("(%s)", (failure->isError() ? "E" : "F"));
}


void 
StdioOutputter::printFailureLocation( SourceLine sourceLine )
{
    if ( !sourceLine.isValid() )
        return;
    
    LOGD("line: %d %s", 
         sourceLine.lineNumber(),
         sourceLine.fileName().c_str());
}


void 
StdioOutputter::printFailureDetail( Exception *thrownException )
{
    LOGD("%s\n", thrownException->message().shortDescription().c_str());
    LOGD("%s", thrownException->message().details().c_str());
}


void 
StdioOutputter::printHeader()
{
    if ( m_result->wasSuccessful() )
        LOGD("\nOK (%d tests)\n", m_result->runTests());
    else
    {
        LOGD("\n");
        printFailureWarning();
        printStatistics();
    }
}


void 
StdioOutputter::printFailureWarning()
{
    LOGD("!!!FAILURES!!!\n");
}


void 
StdioOutputter::printStatistics()
{
    LOGD("Test Results:\n");

    LOGD("Run:  %d   Failures: %d   Errors: %d\n",
         m_result->runTests(),
         m_result->testFailures(),
         m_result->testErrors());
}
