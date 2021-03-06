/*********************************************************************/
/* Copyright (c) 2013-2014, EPFL/Blue Brain Project                  */
/*                          Raphael Dumusc <raphael.dumusc@epfl.ch>  */
/*                          Stefan.Eilemann@epfl.ch                  */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of The University of Texas at Austin.                 */
/*********************************************************************/

#include "StreamPrivate.h"

#include "Stream.h"
#include "StreamSendWorker.h"
#include "PixelStreamSegment.h"
#include "PixelStreamSegmentParameters.h"

#include <iostream>

#include <boost/thread/thread.hpp>
#define SEGMENT_SIZE 512

namespace deflect
{

StreamPrivate::StreamPrivate( Stream* stream, const std::string &name,
                              const std::string& address )
    : name_(name)
    , socket_( address )
    , registeredForEvents_(false)
    , parent_( stream )
    , sendWorker_( 0 )
{
    imageSegmenter_.setNominalSegmentDimensions(SEGMENT_SIZE, SEGMENT_SIZE);

    if( name.empty( ))
        std::cerr << "Invalid Stream name" << std::endl;

    if( socket_.isConnected( ))
    {
        connect(&socket_, SIGNAL(disconnected()), this, SLOT(onDisconnected( )));
        const MessageHeader mh( MESSAGE_TYPE_PIXELSTREAM_OPEN, 0, name_ );
        socket_.send( mh, QByteArray( ));
    }
}

StreamPrivate::~StreamPrivate()
{
    delete sendWorker_;

    if( !socket_.isConnected( ))
        return;

    MessageHeader mh(MESSAGE_TYPE_QUIT, 0, name_);
    socket_.send(mh, QByteArray());

    registeredForEvents_ = false;
}

bool StreamPrivate::send( const ImageWrapper& image )
{
    if( image.compressionPolicy != COMPRESSION_ON &&
        image.pixelFormat != RGBA )
    {
        std::cerr << "Currently, RAW images can only be sent in RGBA format. "
                 "Other formats support remain to be implemented." << std::endl;
        return false;
    }

    const ImageSegmenter::Handler sendFunc =
        boost::bind( &StreamPrivate::sendPixelStreamSegment, this, _1 );
    return imageSegmenter_.generate( image, sendFunc );
}

Stream::Future StreamPrivate::asyncSend( const ImageWrapper& image )
{
    if( !sendWorker_ )
        sendWorker_ = new StreamSendWorker( *this );

    return sendWorker_->enqueueImage( image );
}

bool StreamPrivate::finishFrame()
{
    // Open a window for the PixelStream
    MessageHeader mh(MESSAGE_TYPE_PIXELSTREAM_FINISH_FRAME, 0, name_);
    return socket_.send(mh, QByteArray());
}

bool StreamPrivate::sendPixelStreamSegment(const PixelStreamSegment &segment)
{
    // Create message header
    const uint32_t segmentSize(sizeof(PixelStreamSegmentParameters) +
                               segment.imageData.size());
    MessageHeader mh(MESSAGE_TYPE_PIXELSTREAM, segmentSize, name_);

    // This byte array will hold the message to be sent over the socket
    QByteArray message;

    // Message payload part 1: segment parameters
    message.append( (const char *)(&segment.parameters),
                    sizeof(PixelStreamSegmentParameters));

    // Message payload part 2: image data
    message.append(segment.imageData);

    QMutexLocker locker( &sendLock_ );
    return socket_.send(mh, message);
}

bool StreamPrivate::sendCommand(const QString& command)
{
    QByteArray message;
    message.append(command);

    MessageHeader mh(MESSAGE_TYPE_COMMAND, message.size(), name_);

    return socket_.send(mh, message);
}

void StreamPrivate::onDisconnected()
{
    if( parent_ )
        parent_->disconnected();
}

}
