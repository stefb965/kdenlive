/**************************1*************************************************
                          DocClipRef.cpp  -  description
                             -------------------
    begin                : Fri Apr 12 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "docclipref.h"

#include <kdebug.h>

#include "clipmanager.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "doccliptextfile.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "docclipbase.h"
#include "effectdesc.h"
#include "effectkeyframe.h"
#include "effectparameter.h"
#include "effectparamdesc.h"
#include "effectdoublekeyframe.h"
#include "effectcomplexkeyframe.h"
#include "kdenlivedoc.h"
#include "kdenlivesettings.h"





DocClipRef::DocClipRef(DocClipBase * clip):
m_trackStart(0.0),
m_cropStart(0.0),
m_trackEnd(0.0), m_parentTrack(0), m_trackNum(-1), m_clip(clip), m_thumbcreator(0), startTimer(0), endTimer(0)
{
    if (!clip) {
	kdError() <<
	    "Creating a DocClipRef with no clip - not a very clever thing to do!!!"
	    << endl;
    }

    // If clip is a video, resizing it should update the thumbnails
    if (m_clip->clipType() == DocClipBase::VIDEO || m_clip->clipType() == DocClipBase::AV) {
	m_thumbnail = QPixmap();
        m_endthumbnail = QPixmap();
        m_thumbcreator = new KThumb();
        startTimer = new QTimer( this );
        endTimer = new QTimer( this );
        connect(m_thumbcreator, SIGNAL(thumbReady(int, QPixmap)),this,SLOT(updateThumbnail(int, QPixmap)));
        connect(this, SIGNAL(getClipThumbnail(KURL, int, int, int)), m_thumbcreator, SLOT(getImage(KURL, int, int, int)));

        connect( startTimer, SIGNAL(timeout()), this, SLOT(fetchStartThumbnail()));
        connect( endTimer, SIGNAL(timeout()), this, SLOT(fetchEndThumbnail()));
    }
    else {
    m_thumbnail = referencedClip()->thumbnail();
    m_endthumbnail = m_thumbnail;
    }
}

DocClipRef::~DocClipRef()
{
    delete startTimer;
    delete endTimer;
    delete m_thumbcreator;
}

bool DocClipRef::hasVariableThumbnails()
{
    if ((m_clip->clipType() != DocClipBase::VIDEO && m_clip->clipType() != DocClipBase::AV) || !KdenliveSettings::videothumbnails()) 
        return false;
    return true;
}

void DocClipRef::generateThumbnails()
{
    fetchStartThumbnail();
    fetchEndThumbnail();
}

const GenTime & DocClipRef::trackStart() const
{
    return m_trackStart;
}


void DocClipRef::setTrackStart(const GenTime time)
{
    m_trackStart = time;

    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

const QString & DocClipRef::name() const
{
    return m_clip->name();
}

const QString & DocClipRef::description() const
{
    return m_clip->description();
}

void DocClipRef::setDescription(const QString & description)
{
    m_clip->setDescription(description);
}

void DocClipRef::fetchStartThumbnail()
{
    uint height = KdenliveSettings::videotracksize();
    uint width = height * 1.25;
    emit getClipThumbnail(fileURL(), m_cropStart.frames(25), width, height);
}

void DocClipRef::fetchEndThumbnail()
{
    uint height = KdenliveSettings::videotracksize();
    uint width = height * 1.25;
    emit getClipThumbnail(fileURL(), m_cropStart.frames(25)+cropDuration().frames(25), width, height);
}

void DocClipRef::setCropStartTime(const GenTime & time)
{
    m_cropStart = time;
    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

const GenTime & DocClipRef::cropStartTime() const
{
    return m_cropStart;
}


void DocClipRef::setTrackEnd(const GenTime & time)
{
    m_trackEnd = time;

    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

GenTime DocClipRef::cropDuration() const
{
    return m_trackEnd - m_trackStart;
}

void DocClipRef::updateThumbnail(int frame, QPixmap newpix)
{
    if (m_cropStart.frames(25)+cropDuration().frames(25)-5 < frame)
        m_endthumbnail = newpix;
    else m_thumbnail = newpix;
    if (m_parentTrack) m_parentTrack->refreshLayout();
}


QPixmap DocClipRef::thumbnail(bool end)
{
    if (end) return m_endthumbnail;
    return m_thumbnail;
}

DocClipRef *DocClipRef::
createClip(const EffectDescriptionList & effectList,
    ClipManager & clipManager, const QDomElement & element)
{
    DocClipRef *clip = 0;
    GenTime trackStart;
    GenTime cropStart;
    GenTime cropDuration;
    GenTime trackEnd;
    QString description;
    QValueVector < GenTime > markers;
    EffectStack effectStack;

    kdWarning() << "================================" << endl;
    kdWarning() << "Creating Clip : " << element.ownerDocument().
	toString() << endl;

    int trackNum = 0;

    QDomNode node = element;
    node.normalize();

    if (element.tagName() != "clip") {
	kdWarning() <<
	    "DocClipRef::createClip() element has unknown tagName : " <<
	    element.tagName() << endl;
	return 0;
    }

    DocClipBase *baseClip = clipManager.insertClip(element);

    if (baseClip) {
	clip = new DocClipRef(baseClip);
    }

    QDomElement t;

    QDomNode n = element.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    kdWarning() << "DocClipRef::createClip() tag = " << e.
		tagName() << endl;
	    if (e.tagName() == "avfile") {
		// Do nothing, this is handled via the clipmanage insertClip method above.
	    } else if (e.tagName() == "project") {
		// Do nothing, this is handled via the clipmanage insertClip method above.
	    } else if (e.tagName() == "position") {
		trackNum = e.attribute("track", "-1").toInt();
		trackStart =
		    GenTime(e.attribute("trackstart", "0").toDouble());
		cropStart =
		    GenTime(e.attribute("cropstart", "0").toDouble());
		cropDuration =
		    GenTime(e.attribute("cropduration", "0").toDouble());
		trackEnd =
		    GenTime(e.attribute("trackend", "-1").toDouble());
	    } else if (e.tagName() == "markers") {
		QDomNode markerNode = e.firstChild();
		while (!markerNode.isNull()) {
		    QDomElement markerElement = markerNode.toElement();
		    if (!markerElement.isNull()) {
			if (markerElement.tagName() == "marker") {
			    markers.append(GenTime(markerElement.
				    attribute("time", "0").toDouble()));
			} else {
			    kdWarning() << "Unknown tag " << markerElement.
				tagName() << endl;
			}
		    }
		    markerNode = markerNode.nextSibling();
		}
	    } else if (e.tagName() == "effects") {
		kdWarning() << "Found effects tag" << endl;
		QDomNode effectNode = e.firstChild();
		while (!effectNode.isNull()) {
		    QDomElement effectElement = effectNode.toElement();
		    kdWarning() << "Effect node..." << endl;
		    if (!effectElement.isNull()) {
			kdWarning() << "has tag name " << effectElement.
			    tagName() << endl;
			if (effectElement.tagName() == "effect") {
			    EffectDesc *desc =
				effectList.effectDescription(effectElement.
				attribute("type", QString::null));
			    if (desc) {
				kdWarning() << "Appending effect " <<
				    desc->name() << endl;
				effectStack.
				    append(Effect::createEffect(*desc,
					effectElement));
			    } else {
				kdWarning() << "Unknown effect " <<
				    effectElement.attribute("type",
				    QString::null) << endl;
			    }
			} else {
			    kdWarning() << "Unknown tag " << effectElement.
				tagName() << endl;
			}
		    }
		    effectNode = effectNode.nextSibling();
		}
            }
            else if (e.tagName() == "transitions") {
                t = e;
                kdWarning() << "Found transition tag" << endl;

            }
            else {
//               kdWarning() << "DocClipRef::createClip() unknown tag : " << e.tagName() << endl;
	    }
	}

	n = n.nextSibling();
    }

    if (clip == 0) {
	kdWarning() << "DocClipRef::createClip() unable to create clip" <<
	    endl;
    } else {
	// setup DocClipRef specifics of the clip.
	clip->setTrackStart(trackStart);
	clip->setCropStartTime(cropStart);
	if (trackEnd.seconds() != -1) {
	    clip->setTrackEnd(trackEnd);
	} else {
	    clip->setTrackEnd(trackStart + cropDuration);
	}
	clip->setParentTrack(0, trackNum);
	clip->setSnapMarkers(markers);
	//clip->setDescription(description);
	clip->setEffectStack(effectStack);
        
        // add Transitions
        if (!t.isNull()) {
            QDomNode transitionNode = t.firstChild();
            while (!transitionNode.isNull()) {
                QDomElement transitionElement = transitionNode.toElement();
                kdWarning() << "Effect node..." << endl;
                if (!transitionElement.isNull()) {
                    kdWarning() << "has tag name " << transitionElement.tagName() << endl;
                    if (transitionElement.tagName() == "transition") {
                        GenTime startTime(transitionElement.attribute("start", QString::null).toInt(),25.0);
                        GenTime endTime(transitionElement.attribute("end", QString::null).toInt(),25.0);
                        Transition *transit = new Transition(clip, transitionElement.attribute("type", QString::null), startTime, endTime, transitionElement.attribute("inverted", "0").toInt());
                        
                        // load transition parameters
                        typedef QMap<QString, QString> ParamMap;
                        ParamMap params;
                        for( QDomNode n = transitionElement.firstChild(); !n.isNull(); n = n.nextSibling() )
                        {
                            QDomElement paramElement = n.toElement();
                            params[paramElement.tagName()] = paramElement.attribute("value", QString::null);
                        }
                        if (!params.isEmpty()) transit->setTransitionParameters(params);
                        clip->addTransition(transit);
                    } else {
                        kdWarning() << "Unknown effect " <<transitionElement.attribute("type",QString::null) << endl;
                    }
                } else {
                kdWarning() << "Unknown tag " << transitionElement.tagName() << endl;
                }
                transitionNode = transitionNode.nextSibling();
            }
        }
    }

    return clip;
}

/** Sets the parent track for this clip. */
void DocClipRef::setParentTrack(DocTrackBase * track, const int trackNum)
{
    m_parentTrack = track;
    m_trackNum = trackNum;
}

/** Returns the track number. This is a hint as to which track the clip is on, or should be placed on. */
int DocClipRef::trackNum() const
{
    return m_trackNum;
}

/** Returns the track number in MLT's playlist */
int DocClipRef::playlistTrackNum() const
{
    return m_parentTrack->projectClip()->playlistTrackNum(m_trackNum);
}

/** Returns the end of the clip on the track. A convenience function, equivalent
to trackStart() + cropDuration() */
GenTime DocClipRef::trackEnd() const
{
    return m_trackEnd;
}

/** Returns the parentTrack of this clip. */
DocTrackBase *DocClipRef::parentTrack()
{
    return m_parentTrack;
}

/** Move the clips so that it's trackStart coincides with the time specified. */
void DocClipRef::moveTrackStart(const GenTime & time)
{
    m_trackEnd = m_trackEnd + time - m_trackStart;
    m_trackStart = time;
}

DocClipRef *DocClipRef::clone(const EffectDescriptionList & effectList,
    ClipManager & clipManager)
{
    return createClip(effectList, clipManager, toXML().documentElement());
}

bool DocClipRef::referencesClip(DocClipBase * clip) const
{
    return m_clip->referencesClip(clip);
}

uint DocClipRef::numReferences() const
{
    return m_clip->numReferences();
}

double DocClipRef::framesPerSecond() const
{
    if (m_parentTrack) {
	return m_parentTrack->framesPerSecond();
    } else {
	return m_clip->framesPerSecond();
    }
}

//returns clip video properties -reh
DocClipBase::CLIPTYPE DocClipRef::clipType() const
{
    return m_clip->clipType();
}

uint DocClipRef::clipHeight() const
{
    if (m_clip->isDocClipAVFile())    
        return m_clip->toDocClipAVFile()->clipHeight();
    else if (m_clip->isDocClipTextFile())    
        return m_clip->toDocClipTextFile()->clipHeight();
    // #TODO should return the project height
    return 0;
}

uint DocClipRef::clipWidth() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->clipWidth();
    else if (m_clip->isDocClipTextFile())
        return m_clip->toDocClipTextFile()->clipWidth();
    // #TODO should return the project width
    return 0;
}

QString DocClipRef::avDecompressor()
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->avDecompressor();
    return QString::null;
}

//type ntsc/pal
QString DocClipRef::avSystem()
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->avSystem();
    return QString::null;
}

//returns clip audio properties -reh
uint DocClipRef::audioChannels() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioChannels();
    return 0;
}

QString DocClipRef::audioFormat()
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioFormat();
    return QString::null;
}

uint DocClipRef::audioBits() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioBits();
    return 0;
}

QDomDocument DocClipRef::toXML() const
{
    QDomDocument doc = m_clip->toXML();


    QDomElement clip = doc.documentElement();

    if (clip.tagName() != "clip") {
	kdError() <<
	    "Expected tagname of 'clip' in DocClipRef::toXML(), expect things to go wrong!"
	    << endl;
    }

    QDomElement position = doc.createElement("position");
    position.setAttribute("track", QString::number(trackNum()));
    position.setAttribute("trackstart",
	QString::number(trackStart().seconds(), 'f', 10));
    position.setAttribute("cropstart",
	QString::number(cropStartTime().seconds(), 'f', 10));
    position.setAttribute("cropduration",
	QString::number(cropDuration().seconds(), 'f', 10));
    position.setAttribute("trackend", QString::number(trackEnd().seconds(),
	    'f', 10));

    clip.appendChild(position);

    //  append clip effects
    if (!m_effectStack.isEmpty()) {
	QDomElement effects = doc.createElement("effects");

	EffectStack::iterator itt = m_effectStack.begin();
	while (itt != m_effectStack.end()) {
	    effects.appendChild(doc.importNode((*itt)->toXML().
		    documentElement(), true));
	    ++itt;
	}

	clip.appendChild(effects);
    }
    
    //  append clip transitions
    if (!m_transitionStack.isEmpty()) {
        QDomElement trans = doc.createElement("transitions");

        TransitionStack::iterator itt = m_transitionStack.begin();
        while (itt != m_transitionStack.end()) {
            trans.appendChild(doc.importNode((*itt)->toXML().
                    documentElement(), true));
            ++itt;
        }

        clip.appendChild(trans);
    }

    QDomElement markers = doc.createElement("markers");
    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	QDomElement marker = doc.createElement("marker");
	marker.setAttribute("time",
	    QString::number(m_snapMarkers[count].seconds(), 'f', 10));
	markers.appendChild(marker);
    }
    clip.appendChild(markers);
    doc.appendChild(clip);

    return doc;
}

bool DocClipRef::matchesXML(const QDomElement & element) const
{
    bool result = false;
    if (element.tagName() == "clip") {
	QDomNodeList nodeList = element.elementsByTagName("position");
	if (nodeList.length() > 0) {
	    if (nodeList.length() > 1) {
		kdWarning() <<
		    "More than one position in XML for a docClip? Only matching first one"
		    << endl;
	    }
	    QDomElement positionElement = nodeList.item(0).toElement();

	    if (!positionElement.isNull()) {
		result = true;
		if (positionElement.attribute("track").toInt() !=
		    trackNum())
		    result = false;
		if (positionElement.attribute("trackstart").toInt() !=
		    trackStart().seconds())
		    result = false;
		if (positionElement.attribute("cropstart").toInt() !=
		    cropStartTime().seconds())
		    result = false;
		if (positionElement.attribute("cropduration").toInt() !=
		    cropDuration().seconds())
		    result = false;
		if (positionElement.attribute("trackend").toInt() !=
		    trackEnd().seconds())
		    result = false;
	    }
	}
    }

    return result;
}

const GenTime & DocClipRef::duration() const
{
    return m_clip->duration();
}

bool DocClipRef::durationKnown() const
{
    return m_clip->durationKnown();
}

QDomDocument DocClipRef::generateSceneList()
{
    return m_clip->generateSceneList();
}

QDomDocument DocClipRef::generateXMLTransition(int trackPosition)
{
    QDomDocument transitionList;
    
    if (clipType() == DocClipBase::TEXT && m_clip->toDocClipTextFile()->isTransparent()) {
        QDomElement transition = transitionList.createElement("transition");
        transition.setAttribute("in", trackStart().frames(framesPerSecond()));
        transition.setAttribute("out", trackEnd().frames(framesPerSecond()));
        transition.setAttribute("mlt_service", "composite");
        transition.setAttribute("always_active", "1");
        transition.setAttribute("progressive","1");
        // TODO: we should find a better way to get the previous video track index
        transition.setAttribute("a_track", QString::number(trackPosition - 1));
        // Set b_track to the current clip's track index (+1 because we add a black track at pos 0)
        transition.setAttribute("b_track", QString::number(trackPosition));
        transitionList.appendChild(transition);
    }
    else if (clipType() == DocClipBase::IMAGE && m_clip->toDocClipAVFile()->isTransparent()) {
        QDomElement transition = transitionList.createElement("transition");
        transition.setAttribute("in", trackStart().frames(framesPerSecond()));
        transition.setAttribute("out", trackEnd().frames(framesPerSecond()));
        transition.setAttribute("mlt_service", "composite");
        transition.setAttribute("always_active", "1");
        transition.setAttribute("progressive","1");
        // TODO: we should find a better way to get the previous video track index
        transition.setAttribute("a_track", QString::number(trackPosition - 1));
        // Set b_track to the current clip's track index (+1 because we add a black track at pos 0)
        transition.setAttribute("b_track", QString::number(trackPosition));
        transitionList.appendChild(transition);
    }
    
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        QDomElement transition = transitionList.createElement("transition");
        transition.setAttribute("in", QString::number((*itt)->transitionStartTime().frames(framesPerSecond())));
        transition.setAttribute("out", QString::number((*itt)->transitionEndTime().frames(framesPerSecond())));
        if ((*itt)->transitionType() == "pip") transition.setAttribute("mlt_service", "composite");
        else transition.setAttribute("mlt_service", (*itt)->transitionType());
   
        typedef QMap<QString, QString> ParamMap;
        ParamMap params;
        params = (*itt)->transitionParameters();
        ParamMap::Iterator it;
        for ( it = params.begin(); it != params.end(); ++it ) {
            transition.setAttribute(it.key(), it.data());
        }

        if ((*itt)->invertTransition()) {
            transition.setAttribute("b_track", QString::number((*itt)->transitionStartTrack()));
            transition.setAttribute("a_track", QString::number((*itt)->transitionEndTrack()));
            //transition.setAttribute("b_track", QString::number(trackPosition));
            //transition.setAttribute("a_track", QString::number(trackPosition - 1));
        }
        else {
            transition.setAttribute("a_track", QString::number((*itt)->transitionStartTrack()));
            transition.setAttribute("b_track", QString::number((*itt)->transitionEndTrack()));
            //transition.setAttribute("b_track", QString::number(trackPosition - 1));
            //transition.setAttribute("a_track", QString::number(trackPosition));
        }
        transition.setAttribute("reverse", "1");
        transitionList.appendChild(transition);
        ++itt;
    }
    
        
    return transitionList;
}

QDomDocument DocClipRef::generateXMLClip()
{
    if (m_cropStart == m_trackEnd)
	return QDomDocument();

    QDomDocument sceneList;
 
    QDomElement entry = sceneList.createElement("entry");

/*	if (parentTrack()->clipType() == "Sound") 
	entry.setAttribute("producer", QString("audio_producer") + QString::number(m_clip->toDocClipAVFile()->getId()) );

	else entry.setAttribute("producer", QString("video_producer") + QString::number(m_clip->toDocClipAVFile()->getId()) );*/
    entry.setAttribute("producer", "producer" + QString::number(m_clip->getId()));
    entry.setAttribute("in",
	QString::number(m_cropStart.frames(framesPerSecond())));
    entry.setAttribute("out",
	QString::number((m_cropStart +
		cropDuration()).frames(framesPerSecond())));

    
    // Generate XML for the clip's effects 
    // As a starting point, let's consider effects don't have more than one keyframable parameter.
    // All other parameters are supposed to be "constant", ie a value which can be adjusted by 
    // the user but remains the same during all the clip's duration.
    uint i = 0;
    if (hasEffect())
	while (effectAt(i) != NULL) {
	    Effect *effect = effectAt(i);
	    uint parameterNum = 0;
	    bool hasParameters = false;

	    while (effect->parameter(parameterNum)) {
		uint keyFrameNum = 0;
		uint maxValue;
		uint minValue;

		if (effect->effectDescription().parameter(parameterNum)->type() == "complex") {
		    // Effect has keyframes with several sub-parameters
		    QString startTag, endTag;
		    keyFrameNum =
			effect->parameter(parameterNum)->numKeyFrames();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

		    if (keyFrameNum > 1) {
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
			    QDomElement transition =
				sceneList.createElement("filter");
			    transition.setAttribute("mlt_service",
				effect->effectDescription().tag());
			    uint in =
				m_cropStart.frames(framesPerSecond());
			    uint duration =
				cropDuration().frames(framesPerSecond());
			    transition.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    transition.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));

			    transition.setAttribute(startTag,
				effect->parameter(parameterNum)->
				keyframe(count)->toComplexKeyFrame()->
				processComplexKeyFrame());

			    transition.setAttribute(endTag,
				effect->parameter(parameterNum)->
				keyframe(count +
				    1)->toComplexKeyFrame()->
				processComplexKeyFrame());
			    entry.appendChild(transition);
			}
		    }
		}

		else if (effect->effectDescription().
		    parameter(parameterNum)->type() == "double") {
		    // Effect has one parameter with keyframes
		    QString startTag, endTag;
		    keyFrameNum =
			effect->parameter(parameterNum)->numKeyFrames();
		    maxValue =
			effect->effectDescription().
			parameter(parameterNum)->max();
		    minValue =
			effect->effectDescription().
			parameter(parameterNum)->min();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

		    if (keyFrameNum > 1) {
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
                                QDomElement clipFilter =
				sceneList.createElement("filter");
			    clipFilter.setAttribute("mlt_service",
				effect->effectDescription().tag());
			    uint in =
				m_cropStart.frames(framesPerSecond());
			    uint duration =
				cropDuration().frames(framesPerSecond());
			    clipFilter.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    clipFilter.setAttribute(startTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count)->toDoubleKeyFrame()->
				    value() * maxValue / 100));
			    clipFilter.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));
			    clipFilter.setAttribute(endTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count +
					1)->toDoubleKeyFrame()->value() *
				    maxValue / 100));

			    // Add the other constant parameters if any
			    /*uint parameterNumBis = parameterNum;
			       while (effect->parameter(parameterNumBis)) {
			       transition.setAttribute(effect->effectDescription().parameter(parameterNumBis)->name(),QString::number(effect->effectDescription().parameter(parameterNumBis)->value()));
			       parameterNumBis++;
			       } */
			    entry.appendChild(clipFilter);
			}
		    }
                    }
                else {	// Effect has only constant parameters
                    QDomElement clipFilter =
			sceneList.createElement("filter");
                    clipFilter.setAttribute("mlt_service",
			effect->effectDescription().tag());
		    if (effect->effectDescription().
			parameter(parameterNum)->type() == "constant")
			while (effect->parameter(parameterNum)) {
                        clipFilter.setAttribute(effect->
				effectDescription().
				parameter(parameterNum)->name(),
				QString::number(effect->
				    effectDescription().
				    parameter(parameterNum)->value()));
			    parameterNum++;
			}
                        entry.appendChild(clipFilter);
		}
		parameterNum++;
	    }
	    i++;
	}

    sceneList.appendChild(entry);
    return sceneList;
    //return m_clip->toDocClipAVFile()->sceneToXML(m_cropStart, m_cropStart+cropDuration());        
}


const KURL & DocClipRef::fileURL() const
{
    return m_clip->fileURL();
}

uint DocClipRef::fileSize() const
{
    return m_clip->fileSize();
}

void DocClipRef::populateSceneTimes(QValueVector < GenTime > &toPopulate)
{
    QValueVector < GenTime > sceneTimes;

    m_clip->populateSceneTimes(sceneTimes);

    QValueVector < GenTime >::iterator itt = sceneTimes.begin();
    while (itt != sceneTimes.end()) {
	GenTime convertedTime = (*itt) - cropStartTime();
	if ((convertedTime >= GenTime(0))
	    && (convertedTime <= cropDuration())) {
	    convertedTime += trackStart();
	    toPopulate.append(convertedTime);
	}

	++itt;
    }

    toPopulate.append(trackStart());
    toPopulate.append(trackEnd());
}

QDomDocument DocClipRef::sceneToXML(const GenTime & startTime,
    const GenTime & endTime)
{
    QDomDocument sceneDoc;

    if (m_effectStack.isEmpty()) {
	sceneDoc =
	    m_clip->sceneToXML(startTime - trackStart() + cropStartTime(),
	    endTime - trackStart() + cropStartTime());
    } else {
	// We traverse the effect stack forwards; this let's us add the video clip at the top of the effect stack.
	QDomNode constructedNode =
	    sceneDoc.importNode(m_clip->sceneToXML(startTime -
		trackStart() + cropStartTime(),
		endTime - trackStart() +
		cropStartTime()).documentElement(), true);
	EffectStackIterator itt(m_effectStack);

	while (itt.current()) {
	    QDomElement parElement = sceneDoc.createElement("par");
	    parElement.appendChild(constructedNode);

	    QDomElement effectElement =
		sceneDoc.createElement("videoeffect");
	    effectElement.setAttribute("name",
		(*itt)->effectDescription().name());
	    effectElement.setAttribute("begin", QString::number(0, 'f',
		    10));
	    effectElement.setAttribute("dur",
		QString::number((endTime - startTime).seconds(), 'f', 10));
	    parElement.appendChild(effectElement);

	    constructedNode = parElement;
	    ++itt;
	}
	sceneDoc.appendChild(constructedNode);
    }

    return sceneDoc;
}

void DocClipRef::setSnapMarkers(QValueVector < GenTime > markers)
{
    m_snapMarkers = markers;
    qHeapSort(m_snapMarkers);

    /*QValueVectorIterator<GenTime> itt = markers.begin();

       while(itt != markers.end()) {
       addSnapMarker(*itt);
       ++itt;
       } */
}

QValueVector < GenTime > DocClipRef::snapMarkersOnTrack() const
{
    QValueVector < GenTime > markers;
    markers.reserve(m_snapMarkers.count());

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	markers.append(m_snapMarkers[count] + trackStart() -
	    cropStartTime());
    }

    return markers;
}

void DocClipRef::addSnapMarker(const GenTime & time)
{
    QValueVector < GenTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if (*itt >= time)
	    break;
	++itt;
    }

    if ((itt != m_snapMarkers.end()) && (*itt == time)) {
	kdError() <<
	    "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo"
	    << endl;
    } else {
	m_snapMarkers.insert(itt, time);

	m_parentTrack->notifyClipChanged(this);
    }
}

void DocClipRef::deleteSnapMarker(const GenTime & time)
{
    QValueVector < GenTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if (*itt == time)
	    break;
	++itt;
    }

    if ((itt != m_snapMarkers.end()) && (*itt == time)) {
	m_snapMarkers.erase(itt);
	m_parentTrack->notifyClipChanged(this);
    } else {
	kdError() << "Could not delete marker at time " << time.
	    seconds() << " - it doesn't exist!" << endl;
    }
}

bool DocClipRef::hasSnapMarker(const GenTime & time)
{
    QValueVector < GenTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if (*itt == time)
	    return true;
	++itt;
    }

    return false;
}

void DocClipRef::setCropDuration(const GenTime & time)
{
    setTrackEnd(trackStart() + time);
}

GenTime DocClipRef::findPreviousSnapMarker(const GenTime & time)
{
    QValueVector < GenTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if (*itt >= time)
	    break;
	++itt;
    }

    if (itt != m_snapMarkers.begin()) {
	--itt;
	return *itt;
    } else {
	return GenTime(0);
    }
}

GenTime DocClipRef::findNextSnapMarker(const GenTime & time)
{
    QValueVector < GenTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if (time <= *itt)
	    break;
	++itt;
    }

    if (itt != m_snapMarkers.end()) {
	if (*itt == time) {
	    ++itt;
	}
	if (itt != m_snapMarkers.end()) {
	    return *itt;
	} else {
	    return GenTime(duration());
	}
    } else {
	return GenTime(duration());
    }
}

GenTime DocClipRef::trackMiddleTime() const
{
    return (m_trackStart + m_trackEnd) / 2;
}

QValueVector < GenTime > DocClipRef::snapMarkers() const
{
    return m_snapMarkers;
}

void DocClipRef::addEffect(uint index, Effect * effect)
{
    m_effectStack.insert(index, effect);
}

void DocClipRef::deleteEffect(uint index)
{
    m_effectStack.remove(index);
}


void DocClipRef::setEffectStackSelectedItem(uint ix)
{
    m_effectStack.setSelected(ix);

}

Effect *DocClipRef::selectedEffect()
{
    return m_effectStack.selectedItem();
}

bool DocClipRef::hasEffect()
{
    if (m_effectStack.count() > 0)
	return true;
    return false;
}

Effect *DocClipRef::effectAt(uint index) const
{
    EffectStack::iterator itt = m_effectStack.begin();

    if (index < m_effectStack.count()) {
	for (uint count = 0; count < index; ++count)
	    ++itt;
	return *itt;
    }

    kdError() << "DocClipRef::effectAt() : index out of range" << endl;
    return 0;
}

void DocClipRef::setEffectStack(const EffectStack & effectStack)
{
    m_effectStack = effectStack;
}

const QPixmap & DocClipRef::getAudioImage(int width, int height,
    double frame, double numFrames, int channel)
{
    static QPixmap nullPixmap;

    return nullPixmap;
}

void DocClipRef::addTransition(Transition *transition)
{
    m_transitionStack.append(transition);
    if (m_parentTrack) m_parentTrack->refreshLayout();
}

void DocClipRef::deleteTransition(const GenTime &time)
{
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if (time == GenTime(0.0)) m_transitionStack.remove(*itt);
        else if ((*itt)->transitionStartTime()<time && (*itt)->transitionEndTime()>time)
            m_transitionStack.remove(*itt);
        ++itt;
    }
    if (m_parentTrack) m_parentTrack->refreshLayout();
}

Transition *DocClipRef::transitionAt(const GenTime &time)
{
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if ((*itt)->transitionStartTime()<time && (*itt)->transitionEndTime()>time)
            return (*itt);
        ++itt;
    }
    return 0;
}

void DocClipRef::deleteTransitions()
{
    m_transitionStack.clear();
    if (m_parentTrack) m_parentTrack->refreshLayout();
}

bool DocClipRef::hasTransition(DocClipRef *clip)
{
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if ((*itt)->hasClip(clip)) {
            return true;
        }
        ++itt;
    }
    return false;
}


TransitionStack DocClipRef::clipTransitions()
{
    return m_transitionStack;
}

void DocClipRef::resizeTransitionStart(uint ix, GenTime time)
{
    m_transitionStack.at(ix)->resizeTransitionStart(time);
    if (m_parentTrack) m_parentTrack->refreshLayout();
}

void DocClipRef::resizeTransitionEnd(uint ix, GenTime time)
{
    m_transitionStack.at(ix)->resizeTransitionEnd(time);
    if (m_parentTrack) m_parentTrack->refreshLayout();
}

void DocClipRef::moveTransition(uint ix, GenTime time)
{
    m_transitionStack.at(ix)->moveTransition(time);
    if (m_parentTrack) m_parentTrack->refreshLayout();
}
