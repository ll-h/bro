// See the file "COPYING" in the main distribution directory for copyright.

#include "Ascii.h"
#include "NetVar.h"

#include <fstream>
#include <sstream>

#include "../../threading/SerialTypes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

using namespace input::reader;
using threading::Value;
using threading::Field;

FieldMapping::FieldMapping(const string& arg_name, const TypeTag& arg_type, int arg_position)
	: name(arg_name), type(arg_type)
	{
	position = arg_position;
	secondary_position = -1;
	present = true;
	}

FieldMapping::FieldMapping(const string& arg_name, const TypeTag& arg_type,
		const TypeTag& arg_subtype, int arg_position)
	: name(arg_name), type(arg_type), subtype(arg_subtype)
	{
	position = arg_position;
	secondary_position = -1;
	present = true;
	}

FieldMapping::FieldMapping(const FieldMapping& arg)
	: name(arg.name), type(arg.type), subtype(arg.subtype), present(arg.present)
	{
	position = arg.position;
	secondary_position = arg.secondary_position;
	}

FieldMapping FieldMapping::subType()
	{
	return FieldMapping(name, subtype, position);
	}

Ascii::Ascii(ReaderFrontend *frontend) : ReaderBackend(frontend)
	{
	file = 0;

	separator.assign( (const char*) BifConst::InputAscii::separator->Bytes(),
			  BifConst::InputAscii::separator->Len());

	if ( separator.size() != 1 )
		Error("separator length has to be 1. Separator will be truncated.");

	set_separator.assign( (const char*) BifConst::InputAscii::set_separator->Bytes(),
		              BifConst::InputAscii::set_separator->Len());

	if ( set_separator.size() != 1 )
		Error("set_separator length has to be 1. Separator will be truncated.");

	empty_field.assign( (const char*) BifConst::InputAscii::empty_field->Bytes(),
			    BifConst::InputAscii::empty_field->Len());

	unset_field.assign( (const char*) BifConst::InputAscii::unset_field->Bytes(),
			    BifConst::InputAscii::unset_field->Len());

	ascii = new AsciiFormatter(this, AsciiFormatter::SeparatorInfo(set_separator, unset_field, empty_field));
}

Ascii::~Ascii()
	{
	DoClose();
	delete ascii; 
	}

void Ascii::DoClose()
	{
	if ( file != 0 )
		{
		file->close();
		delete(file);
		file = 0;
		}
	}

bool Ascii::DoInit(const ReaderInfo& info, int num_fields, const Field* const* fields)
	{
	mtime = 0;

	file = new ifstream(info.source);
	if ( ! file->is_open() )
		{
		Error(Fmt("Init: cannot open %s", info.source));
		delete(file);
		file = 0;
		return false;
		}

	if ( ReadHeader(false) == false )
		{
		Error(Fmt("Init: cannot open %s; headers are incorrect", info.source));
		file->close();
		delete(file);
		file = 0;
		return false;
		}

	DoUpdate();

	return true;
	}


bool Ascii::ReadHeader(bool useCached)
	{
	// try to read the header line...
	string line;
	map<string, uint32_t> ifields;

	if ( ! useCached )
		{
		if ( ! GetLine(line) )
			{
			Error("could not read first line");
			return false;
			}

		headerline = line;
		}

	else
		line = headerline;

	// construct list of field names.
	istringstream splitstream(line);
	int pos=0;
	while ( splitstream )
		{
		string s;
		if ( ! getline(splitstream, s, separator[0]))
			break;

		ifields[s] = pos;
		pos++;
		}

	// printf("Updating fields from description %s\n", line.c_str());
	columnMap.clear();

	for ( int i = 0; i < NumFields(); i++ )
		{
		const Field* field = Fields()[i];

		map<string, uint32_t>::iterator fit = ifields.find(field->name);
		if ( fit == ifields.end() )
			{
			if ( field->optional )
				{
				// we do not really need this field. mark it as not present and always send an undef back.
				FieldMapping f(field->name, field->type, field->subtype, -1);
				f.present = false;
				columnMap.push_back(f);
				continue;
				}

			Error(Fmt("Did not find requested field %s in input data file %s.",
				  field->name, Info().source));
			return false;
			}


		FieldMapping f(field->name, field->type, field->subtype, ifields[field->name]);

		if ( field->secondary_name && strlen(field->secondary_name) != 0 )
			{
			map<string, uint32_t>::iterator fit2 = ifields.find(field->secondary_name);
			if ( fit2 == ifields.end() )
				{
				Error(Fmt("Could not find requested port type field %s in input data file.",
					  field->secondary_name));
				return false;
				}

			f.secondary_position = ifields[field->secondary_name];
			}

		columnMap.push_back(f);
		}


	// well, that seems to have worked...
	return true;
	}

bool Ascii::GetLine(string& str)
	{
	while ( getline(*file, str) )
       		{
		if ( str[0] != '#' )
			return true;

		if ( ( str.length() > 8 ) && ( str.compare(0,7, "#fields") == 0 ) && ( str[7] == separator[0] ) )
			{
			str = str.substr(8);
			return true;
			}
		}

	return false;
	}

// read the entire file and send appropriate thingies back to InputMgr
bool Ascii::DoUpdate()
	{
	switch ( Info().mode ) {
		case MODE_REREAD:
			{
			// check if the file has changed
			struct stat sb;
			if ( stat(Info().source, &sb) == -1 )
				{
				Error(Fmt("Could not get stat for %s", Info().source));
				return false;
				}

			if ( sb.st_mtime <= mtime ) // no change
				return true;

			mtime = sb.st_mtime;
			// file changed. reread.

			// fallthrough
			}

		case MODE_MANUAL:
		case MODE_STREAM:
			{
			// dirty, fix me. (well, apparently after trying seeking, etc
			// - this is not that bad)
			if ( file && file->is_open() )
				{
				if ( Info().mode == MODE_STREAM )
					{
					file->clear(); // remove end of file evil bits
					if ( !ReadHeader(true) )
						return false; // header reading failed

					break;
					}

				file->close();
				delete file;
				file = 0;
				}

			file = new ifstream(Info().source);
			if ( ! file->is_open() )
				{
				Error(Fmt("cannot open %s", Info().source));
				return false;
				}

			if ( ReadHeader(false) == false )
				{
				return false;
				}

			break;
			}

		default:
			assert(false);

		}

	string line;
	while ( GetLine(line ) )
		{
		// split on tabs
		bool error = false;
		istringstream splitstream(line);

		map<int, string> stringfields;
		int pos = 0;
		while ( splitstream )
			{
			string s;
			if ( ! getline(splitstream, s, separator[0]) )
				break;

			stringfields[pos] = s;
			pos++;
			}

		pos--; // for easy comparisons of max element.

		Value** fields = new Value*[NumFields()];

		int fpos = 0;
		for ( vector<FieldMapping>::iterator fit = columnMap.begin();
			fit != columnMap.end();
			fit++ )
			{

			if ( ! fit->present )
				{
				// add non-present field
				fields[fpos] =  new Value((*fit).type, false);
				fpos++;
				continue;
				}

			assert(fit->position >= 0 );

			if ( (*fit).position > pos || (*fit).secondary_position > pos )
				{
				Error(Fmt("Not enough fields in line %s. Found %d fields, want positions %d and %d",
					  line.c_str(), pos,  (*fit).position, (*fit).secondary_position));
				return false;
				}

			Value* val = ascii->ParseValue(stringfields[(*fit).position], (*fit).name, (*fit).type, (*fit).subtype);

			if ( val == 0 )
				{
				Error(Fmt("Could not convert line '%s' to Val. Ignoring line.", line.c_str()));
				error = true;
				break;
				}

			if ( (*fit).secondary_position != -1 )
				{
				// we have a port definition :)
				assert(val->type == TYPE_PORT );
				//	Error(Fmt("Got type %d != PORT with secondary position!", val->type));

				val->val.port_val.proto = ascii->ParseProto(stringfields[(*fit).secondary_position]);
				}

			fields[fpos] = val;

			fpos++;
			}

		if ( error )
			{
			// Encountered non-fatal error, ignoring line. But
			// first, delete all successfully read fields and the
			// array structure.

			for ( int i = 0; i < fpos; i++ )
				delete fields[i];

			delete [] fields;
			continue;
			}

		//printf("fpos: %d, second.num_fields: %d\n", fpos, (*it).second.num_fields);
		assert ( fpos == NumFields() );

		if ( Info().mode  == MODE_STREAM )
			Put(fields);
		else
			SendEntry(fields);
		}

	if ( Info().mode != MODE_STREAM )
		EndCurrentSend();

	return true;
	}

bool Ascii::DoHeartbeat(double network_time, double current_time)
{
	switch ( Info().mode  ) {
		case MODE_MANUAL:
			// yay, we do nothing :)
			break;

		case MODE_REREAD:
		case MODE_STREAM:
			Update(); // call update and not DoUpdate, because update
				  // checks disabled.
			break;

		default:
			assert(false);
	}

	return true;
	}

