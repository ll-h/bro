%include binpac.pac
%include bro.pac

%extern{
#include "goose_pac.h"

#include "events.bif.h"
%}


analyzer GOOSE withcontext {
};

%include goose-protocol.pac

# === Exporting to BroVal objects ===

function goosePdu_as_val(pdu : IECGoosePdu): RecordVal
%{
	RecordVal * result = new RecordVal(BifType::Record::GOOSE::PDU);

	result->Assign(0, bytestring_to_val(${pdu.gocbRef.str}));
	result->Assign(1, new Val(${pdu.timeAllowedToLive.gooseUInt.val}, TYPE_COUNT));
	// goID is optional
	if(${pdu.has_goID}) // check if pointer is NULL
	{
		result->Assign(2, bytestring_to_val(${pdu.goIDAndT.goID}));
		result->Assign(3, gooseT_as_val(${pdu.goIDAndT.t.val}));
	}
	else
		result->Assign(3, gooseT_as_val(${pdu.t}));

	result->Assign(4, new Val(${pdu.stNum.gooseUInt.val}, TYPE_COUNT));
	result->Assign(5, new Val(${pdu.sqNum.gooseUInt.val}, TYPE_COUNT));

	if(${pdu.testAndConfRev.boolValIsPresent})
		result->Assign(6, new Val(${pdu.testAndConfRev.boolVal}, TYPE_BOOL));
		
	result->Assign(7, new Val(${pdu.testAndConfRev.uintVal}, TYPE_COUNT));

	if(${pdu.ndsComAndNumDatSetEntries.boolValIsPresent})
		result->Assign(8, new Val(${pdu.ndsComAndNumDatSetEntries.boolVal}, TYPE_BOOL));
		
	result->Assign(9, new Val(${pdu.ndsComAndNumDatSetEntries.uintVal}, TYPE_COUNT));

	result->Assign(10, goose_data_array_as_val(${pdu.allData}));
	
	return result;
%}
