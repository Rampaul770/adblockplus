#include <stdexcept>
#include <wctype.h>

#include "process.h"

//-------------------------------------------------------
// Windows_Handle
//-------------------------------------------------------
Windows_Handle::Windows_Handle( HANDLE h ) 
  : handle( h ) 
{
  validate_handle() ;
}

Windows_Handle::~Windows_Handle()
{
  CloseHandle( handle ) ;
}

void Windows_Handle::operator=( HANDLE h )
{
  this -> ~Windows_Handle() ;
  handle = h ;
  validate_handle() ;
}

void Windows_Handle::validate_handle()
{
  if ( handle == INVALID_HANDLE_VALUE )
  {
    throw std::runtime_error( "Invalid handle" ) ;
  }
}

//-------------------------------------------------------
// process_by_name_CI
//-------------------------------------------------------
process_by_name_CI::process_by_name_CI( const wchar_t * name )
  : name( name ), length( wcslen( name ) )
{}

bool process_by_name_CI::operator()( const PROCESSENTRY32W & process )
{
  return 0 == wcsncmpi( process.szExeFile, name, length ) ;
}

//-------------------------------------------------------
// process_by_any_exe_name_CI
//-------------------------------------------------------
bool process_by_any_exe_name_CI::operator()( const PROCESSENTRY32W & process )
{
  return names.find( process.szExeFile ) != names.end() ;
}

//-------------------------------------------------------
// wcscmpi
//-------------------------------------------------------
int wcscmpi( const wchar_t * s1, const wchar_t * s2 )
{
  // Note: Equality of character sequences is case-insensitive in all predicates below.
  // Loop invariant: s1[0..j) == s2[0..j)
  const size_t LIMIT( 4294967295 ) ; // Runaway limit of 2^32 - 1 should be acceptably long.
  for ( size_t j = 0 ; j < LIMIT ; ++j )
  {
    wchar_t c1 = towupper( *s1++ ) ;
    wchar_t c2 = towupper( *s2++ ) ;
    if ( c1 != c2 )
    {
      // Map to -1/+1 because c2 - c1 may not fit into an 'int'.
      return ( c1 < c2 ) ? -1 : 1 ;
    }
    else
    {
      if ( c1 == L'\0' )
      {
	// Assert length( s1 ) == length( s2 ) == j
	// Assert strings are equal at length < n
	return 0 ;
      }
    }
  }
  // Assert j == LIMIT
  // Assert s1[0..LIMIT) == s2[0..LIMIT)
  // Both strings are longer than 64K, which violates the precondition
  throw std::runtime_error( "String arguments too long for wcscmpi" ) ;
}

//-------------------------------------------------------
// wcsncmpi
//-------------------------------------------------------
int wcsncmpi( const wchar_t * s1, const wchar_t * s2, unsigned int n )
{
  // Note: Equality of character sequences is case-insensitive in all predicates below.
  // Loop invariant: s1[0..j) == s2[0..j)
  for ( unsigned int j = 0 ; j < n ; ++j )
  {
    wchar_t c1 = towupper( *s1++ ) ;
    wchar_t c2 = towupper( *s2++ ) ;
    if ( c1 != c2 )
    {
      // Map to -1/+1 because c2 - c1 may not fit into an 'int'.
      return ( c1 < c2 ) ? -1 : 1 ;
    }
    else
    {
      if ( c1 == L'\0' )
      {
	// Assert length( s1 ) == length( s2 ) == j
	// Assert strings are equal at length < n
	return 0 ;
      }
    }
  }
  // Assert j == n
  // Assert s1[0..n) == s2[0..n)
  // The semantics of n-compare ignore everything after the first 'n' characters.
  return 0 ;
}

//-------------------------------------------------------
// creator_process
//-------------------------------------------------------
DWORD creator_process( HWND window )
{
  DWORD pid ;
  DWORD r = GetWindowThreadProcessId( window, & pid ) ;
  if ( r == 0 )
  {
    // Assert GetWindowThreadProcessId returned an error
    // If the window handle is invalid, we end up here.
    throw std::runtime_error( "" ) ;
  }
  return pid ;
}

//-------------------------------------------------------
// Snapshot
//-------------------------------------------------------
Snapshot::Snapshot()
  : handle( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) )
{
  process.dwSize = sizeof( PROCESSENTRY32W ) ;
}

PROCESSENTRY32W * Snapshot::begin()
{
  return ::Process32FirstW( handle, & process ) ? ( & process ) : 0 ;
}

PROCESSENTRY32W * Snapshot::next()
{
  return ::Process32NextW( handle, & process ) ? ( & process ) : 0 ;
}

void Snapshot::refresh()
{
  handle = ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) ;
}
