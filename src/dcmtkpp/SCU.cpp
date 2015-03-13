/*************************************************************************
 * dcmtkpp - Copyright (C) Universite de Strasbourg
 * Distributed under the terms of the CeCILL-B license, as published by
 * the CEA-CNRS-INRIA. Refer to the LICENSE file or to
 * http://www.cecill.info/licences/Licence_CeCILL-B_V1-en.html
 * for details.
 ************************************************************************/

#include "dcmtkpp/SCU.h"

#include <functional>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmnet/cond.h>
#include <dcmtk/dcmnet/dimse.h>

#include "dcmtkpp/Exception.h"

namespace dcmtkpp
{

SCU
::SCU()
: _association(NULL), _affected_sop_class("")
{
    // Nothing else
}

SCU
::SCU(SCU const & other)
:  _association(other._association), _affected_sop_class(other._affected_sop_class)
{
    // Nothing else
}

SCU const &
SCU
::operator=(SCU const & other)
{
    if(this != &other)
    {
        this->set_association(other.get_association());
        this->set_affected_sop_class(other.get_affected_sop_class());
    }
    
    return *this;
}

SCU
::~SCU()
{
    // Nothing to do.
}

Network *
SCU
::get_network() const
{
    return this->_network;
}

void
SCU
::set_network(Network * network)
{
    this->_network = network;
}

Association *
SCU
::get_association() const
{
    return this->_association;
}

void
SCU
::set_association(Association * association)
{
    this->_association = association;
}

std::string const &
SCU
::get_affected_sop_class() const
{
    return this->_affected_sop_class;
}
    
void 
SCU
::set_affected_sop_class(std::string const & sop_class)
{
    this->_affected_sop_class = sop_class;
}

void
SCU
::echo() const
{
    if(this->_association == NULL || !this->_association->is_associated())
    {
        throw Exception("Not associated");
    }
    
    DIC_US const message_id = this->_association->get_association()->nextMsgID++;
    DIC_US status;
    DcmDataset *detail = NULL;
    // FIXME: block mode and timeout
    OFCondition const condition = DIMSE_echoUser(
        this->_association->get_association(), message_id, DIMSE_BLOCKING, 30, 
        &status, &detail);
    
    if(condition.bad())
    {
        OFString empty;
        throw Exception(DimseCondition::dump(empty, condition).c_str());
    }
}

void 
SCU
::_progress_callback_wrapper(void * data, unsigned long bytes_count)
{
    ProgressCallbackData * encapsulated = 
        reinterpret_cast<ProgressCallbackData*>(data);
    encapsulated->callback(encapsulated->data, bytes_count);
}

T_ASC_PresentationContextID
SCU
::_find_presentation_context() const
{
    T_ASC_PresentationContextID const presentation_id = 
        ASC_findAcceptedPresentationContextID(
            this->_association->get_association(), 
            this->_affected_sop_class.c_str());
    if(presentation_id == 0) 
    {
        throw Exception("No Presentation Context for Get Operation");
    }
    
    return presentation_id;
}

std::pair<T_ASC_PresentationContextID, T_DIMSE_Message>
SCU
::_receive_command(T_DIMSE_BlockingMode block_mode) const
{
    std::pair<T_ASC_PresentationContextID, T_DIMSE_Message> result;
    
    result.first = -1;
    bzero((char*)&result.second, sizeof(result.second));
    
    OFCondition const condition = DIMSE_receiveCommand(
        this->_association->get_association(), block_mode, 
        this->_network->get_timeout(), 
        &result.first, &result.second, 
        NULL /*statusDetail*/, NULL /*commandSet*/);
    
    if(condition.bad())
    {
        throw Exception(condition);
    }
    
    return result;
}

std::pair<T_ASC_PresentationContextID, DcmDataset *>
SCU
::_receive_dataset(
    T_DIMSE_BlockingMode block_mode,
    ProgressCallback callback, void* callback_data) const
{
    std::pair<T_ASC_PresentationContextID, DcmDataset *> result;
    result.second = NULL;
    
    // Encapsulate the callback and its data
    ProgressCallbackData encapsulated;
    if(callback != NULL)
    {
        encapsulated.callback = callback;
        encapsulated.data = callback_data;
    }
    
    OFCondition const condition = DIMSE_receiveDataSetInMemory(
        this->_association->get_association(), block_mode, 
        this->_network->get_timeout(), 
        &result.first, &result.second, 
        (callback != NULL)?(SCU::_progress_callback_wrapper):NULL, 
        (callback != NULL)?(&encapsulated):NULL
    );
    
    if(condition.bad())
    {
        throw Exception(condition);
    }
    
    return result;
}

}
