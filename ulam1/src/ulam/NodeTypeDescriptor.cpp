#include <stdlib.h>
#include "NodeTypeDescriptor.h"
#include "CompilerState.h"


namespace MFM {

  NodeTypeDescriptor::NodeTypeDescriptor(Token tokarg, UTI auti, CompilerState & state) : Node(state), m_typeTok(tokarg), m_uti(auti), m_ready(false), m_unknownBitsizeSubtree(NULL), m_refType(ALT_NOT), m_referencedUTI(Nouti)
  {
    setNodeLocation(m_typeTok.m_locator);
  }

  // use by ALT_AS.
  NodeTypeDescriptor::NodeTypeDescriptor(Token tokarg, UTI auti, CompilerState & state, ALT refarg, UTI referencedUTIarg) : Node(state), m_typeTok(tokarg), m_uti(auti), m_ready(false), m_unknownBitsizeSubtree(NULL), m_refType(refarg), m_referencedUTI(referencedUTIarg)
  {
    setNodeLocation(m_typeTok.m_locator);
  }

  //since there's no assoc symbol, we map the m_uti here (e.g. S(x,y).sizeof nodeterminalproxy)
  NodeTypeDescriptor::NodeTypeDescriptor(const NodeTypeDescriptor& ref) : Node(ref), m_typeTok(ref.m_typeTok), m_uti(m_state.mapIncompleteUTIForCurrentClassInstance(ref.m_uti)), m_ready(false), m_unknownBitsizeSubtree(NULL), m_refType(ref.m_refType), m_referencedUTI(m_state.mapIncompleteUTIForCurrentClassInstance(ref.m_referencedUTI))
  {
    if(ref.m_unknownBitsizeSubtree)
      m_unknownBitsizeSubtree = new NodeTypeBitsize(*ref.m_unknownBitsizeSubtree); //mapped UTI?
  }

  NodeTypeDescriptor::~NodeTypeDescriptor()
  {
    delete m_unknownBitsizeSubtree;
    m_unknownBitsizeSubtree = NULL;
  } //destructor

  Node * NodeTypeDescriptor::instantiate()
  {
    return new NodeTypeDescriptor(*this);
  }

  void NodeTypeDescriptor::updateLineage(NNO pno)
  {
    setYourParentNo(pno);
    if(m_unknownBitsizeSubtree)
      m_unknownBitsizeSubtree->updateLineage(getNodeNo());
  } //updateLineage

  bool NodeTypeDescriptor::findNodeNo(NNO n, Node *& foundNode)
  {
    if(Node::findNodeNo(n, foundNode))
      return true;
    if(m_unknownBitsizeSubtree && m_unknownBitsizeSubtree->findNodeNo(n, foundNode))
      return true;
    return false;
  } //findNodeNo

  void NodeTypeDescriptor::printPostfix(File * fp)
  {
    fp->write(getName());
  }

  const char * NodeTypeDescriptor::getName()
  {
    return m_state.getTokenDataAsString(&m_typeTok).c_str();
  } //getName

  const std::string NodeTypeDescriptor::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  void NodeTypeDescriptor::linkConstantExpressionBitsize(NodeTypeBitsize * ceForBitSize)
  {
    assert(!m_unknownBitsizeSubtree);
    m_unknownBitsizeSubtree = ceForBitSize; //tfr owner
  } //linkConstantExpressionBitsize

  bool NodeTypeDescriptor::isReadyType()
  {
    return m_ready;
  }

  UTI NodeTypeDescriptor::givenUTI()
  {
    return m_uti;
  }

  UTI NodeTypeDescriptor::getReferencedUTI()
  {
    return m_referencedUTI;
  }

  ALT NodeTypeDescriptor::getReferenceType()
  {
    return m_refType;
  }

  void NodeTypeDescriptor::setReferenceType(ALT refarg, UTI referencedUTI)
  {
    m_refType = refarg;
    m_referencedUTI = referencedUTI;
  }

  void NodeTypeDescriptor::setReferenceType(ALT refarg, UTI referencedUTI, UTI refUTI)
  {
    setReferenceType(refarg, referencedUTI);
    m_uti = refUTI; //new given as ref UTI
  }

  UTI NodeTypeDescriptor::checkAndLabelType()
  {
    if(isReadyType())
      return getNodeType();

    UTI it = givenUTI();
    if(resolveType(it)) //ref
      {
	setNodeType(it);
	m_ready = true; //set here!!!
      }
    else if(it == Hzy)
      {
	setNodeType(Hzy);
	m_state.setGoAgain();
      }
    else
      setNodeType(it);

    return getNodeType();
  } //checkAndLabelType

  bool NodeTypeDescriptor::resolveType(UTI& rtnuti)
  {
    bool rtnb = false;
    if(isReadyType())
      {
	rtnuti = getNodeType();
	return true;
      }

    // not node select, we are the leaf Type: a typedef, class or primitive scalar.
    UTI nuti = givenUTI();

    if(m_refType != ALT_NOT)
      rtnb = resolveReferenceType(nuti); //may update nuti

    if(!m_state.isComplete(nuti))
      {
	// if Nav, use token
	UTI mappedUTI = nuti;
	UTI cuti = m_state.getCompileThisIdx();

	// the symbol associated with this type, was mapped during instantiation
	// since we're call AFTER that (not during), we can look up our
	// new UTI and pass that on up the line of NodeType Selects, if any.
	if(m_state.mappedIncompleteUTI(cuti, nuti, mappedUTI))
	  {
	    std::ostringstream msg;
	    msg << "Substituting Mapped UTI" << mappedUTI;
	    msg << ", " << m_state.getUlamTypeNameBriefByIndex(mappedUTI).c_str();
	    msg << " for incomplete descriptor type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(nuti).c_str();
	    msg << "' UTI" << nuti << " while labeling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	    nuti = mappedUTI;
	  }

	if(!m_state.isComplete(nuti)) //reloads to recheck for debug message
	  {
	    std::ostringstream msg;
	    msg << "Incomplete descriptor for type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(nuti).c_str();
	    msg << " UTI" << nuti << " while labeling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	  }
      }

    UlamType * nut = m_state.getUlamTypeByIndex(nuti);
    ULAMTYPE etyp = nut->getUlamTypeEnum();
    if(etyp == Class)
      {
	rtnb = resolveClassType(nuti);
	if(rtnb)
	  rtnuti = nuti;
      }
    else if(etyp == Holder)
      {
	//non-class reference, handled in resolveReferenceType
	if(m_refType == ALT_NOT)
	  {
	    // non-class, non-ref
	    if(m_state.statusUnknownTypeInThisClassResolver(nuti))
	      rtnuti = nuti; //resolved!
	    else
	      rtnuti = Hzy;
	  }
      }
    else
      {
	//primitive, or array typedef
	if(!m_unknownBitsizeSubtree)
	  {
	    if(!m_state.okUTItoContinue(nuti))
	      {
		//use default primitive bitsize; (assumes scalar)
		rtnuti = m_state.makeUlamType(m_typeTok, ULAMTYPE_DEFAULTBITSIZE[etyp], NONARRAYSIZE, Nouti);
		rtnb = true;
	      }
	    else if(m_state.isComplete(nuti))
	      {
		rtnuti = nuti;
		rtnb = true;
	      }
	    else
	      rtnuti = Hzy;
	    //else mapped?
	  }
	else
	  {
	    //primitive with possible unknown bit size
	    rtnb = resolveTypeBitsize(nuti);
	    rtnuti = nuti;
	  }
      }
    return rtnb;
  } //resolveType

  bool NodeTypeDescriptor::resolveReferenceType(UTI& rtnuti)
  {
    bool rtnb = false;

    UTI nuti = givenUTI();
    UTI cuti = m_state.getCompileThisIdx();

    assert(m_refType != ALT_NOT);

    //if reference is not complete, but its deref is, use its sizes to complete us.
    UlamType * nut = m_state.getUlamTypeByIndex(nuti);
    UTI derefuti = getReferencedUTI();
    if(!m_state.okUTItoContinue(derefuti))
      {
	if(nut->getUlamTypeEnum() == Class)
	  derefuti = nut->getUlamKeyTypeSignature().getUlamKeyTypeSignatureClassInstanceIdx();
	else
	  derefuti = m_state.getUlamTypeAsDeref(nuti);
      }

    if(!m_state.isComplete(derefuti))
      {
	// if Nav, use token
	UTI mappedUTI = derefuti;

	// the symbol associated with this type, was mapped during instantiation
	// since we're call AFTER that (not during), we can look up our
	// new UTI and pass that on up the line of NodeType Selects, if any.
	if(m_state.mappedIncompleteUTI(cuti, derefuti, mappedUTI))
	  {
	    std::ostringstream msg;
	    msg << "Substituting Mapped UTI" << mappedUTI;
	    msg << ", " << m_state.getUlamTypeNameBriefByIndex(mappedUTI).c_str();
	    msg << " for incomplete de-ref descriptor type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(derefuti).c_str();
	    msg << "' UTI" << derefuti << " while labeling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	    derefuti = mappedUTI;
	  }
      } //incomplete deref

    if(m_state.isComplete(derefuti))
      {
	if(nut->getReferenceType() != m_refType)
	  {
	    nuti = m_state.getUlamTypeAsRef(nuti, m_refType);
	    nut =  m_state.getUlamTypeByIndex(nuti);
	    setReferenceType(m_refType, derefuti, nuti); //updates given too!
	  }
	else
	  setReferenceType(m_refType, derefuti);

	UlamType * derefut = m_state.getUlamTypeByIndex(derefuti);
	// we might have set the size of a holder ref. still a holder. darn.
	if(!m_state.completeAReferenceTypeWith(nuti, derefuti) && m_state.isHolder(nuti))
	  {
	    ULAMTYPE bUT = derefut->getUlamTypeEnum();
	    if(bUT == Class)
	      m_state.makeClassFromHolder(nuti, m_typeTok); //token for locator

	    UlamKeyTypeSignature derefkey = derefut->getUlamKeyTypeSignature();
	    UlamKeyTypeSignature newkey(derefkey.getUlamKeyTypeSignatureNameId(), derefkey.getUlamKeyTypeSignatureBitSize(), derefkey.getUlamKeyTypeSignatureArraySize(), derefkey.getUlamKeyTypeSignatureClassInstanceIdx(), m_refType);
	    m_state.makeUlamTypeFromHolder(newkey, bUT, nuti); //keeps nuti
	  }
	rtnb = true;
      } //complete deref
    //else deref not complete, t.f. nuti isn't changed

    //if(rtnb)
      rtnuti = nuti;
    return rtnb;
  } //resolveReferenceType

  bool NodeTypeDescriptor::resolveClassType(UTI& rtnuti)
  {
    bool rtnb = false;
    UTI nuti = rtnuti;
    UlamType * nut = m_state.getUlamTypeByIndex(nuti);
    ULAMTYPE etyp = nut->getUlamTypeEnum();
    assert(etyp == Class);

    ULAMCLASSTYPE nclasstype = nut->getUlamClass();
    if(nut->isComplete())
      {
	rtnuti = nuti;
	rtnb = true;
      } //else we're not ready!!
    else if(nclasstype == UC_UNSEEN)
      {
	UTI tduti = Nouti;
	UTI tmpforscalaruti = Nouti;
	bool isTypedef = m_state.getUlamTypeByTypedefName(m_typeTok.m_dataindex, tduti, tmpforscalaruti);

	if(isTypedef)
	  {
	    //unseen typedef's appear like Class basetype, until seen
	    //wait until complete to re-key..
	    // want arraysize if non-scalar
	    std::ostringstream msg;
	    if(m_state.isComplete(tduti))
	      {
		UlamType * tut = m_state.getUlamTypeByIndex(tduti);
		UlamKeyTypeSignature tdkey = tut->getUlamKeyTypeSignature();
		UlamKeyTypeSignature newkey(tdkey.getUlamKeyTypeSignatureNameId(), tut->getBitSize(), tut->getArraySize(), 0, tut->getReferenceType());
		m_state.makeUlamTypeFromHolder(newkey, tut->getUlamTypeEnum(), nuti);
		rtnuti = tduti; //reset
		rtnb = true;
		msg << "RESET ";
	      }
	    else
	      rtnuti = Hzy;

	    msg << "Unseen Class was a typedef for: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(tduti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	  }
	else
	  {
	    bool isAnonymousClass = (nut->isHolder() || !m_state.isARootUTI(nuti));
	    UTI auti = nuti;

	    if(nut->isHolder())
	      {
		if(!m_state.statusUnknownTypeInThisClassResolver(nuti))
		  auti = Hzy;
	      }
	    else if(!m_state.isARootUTI(nuti))
	      {
		AssertBool isAlias = m_state.findaUTIAlias(nuti, auti);
		assert(isAlias);
	      }

	    std::ostringstream msg;
	    msg << "UNSEEN Class and incomplete descriptor for type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(nuti).c_str();
	    if(isAnonymousClass)
	      {
		msg << " replaced with type: (UTI" << auti << ")";
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
		rtnuti = auti; //was Hzy
	      }
	    else
	      {
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
		rtnuti = Nav;
	      }
	  }
      }
    else
      rtnuti = Hzy;
    return rtnb;
  } //resolveClassType

  bool NodeTypeDescriptor::resolveTypeBitsize(UTI& rtnuti)
  {
    UTI auti = rtnuti;
    UlamType * ut = m_state.getUlamTypeByIndex(auti);
    ULAMTYPE etype = ut->getUlamTypeEnum();
    if(m_unknownBitsizeSubtree)
      {
	s32 bs = UNKNOWNSIZE;
	//primitive with possible unknown bit sizes.
	bool rtnb = m_unknownBitsizeSubtree->getTypeBitSizeInParen(bs, etype, auti); //eval
	if(rtnb)
	  {
	    if(bs < 0)
	      {
		std::ostringstream msg;
		msg << "Type Bitsize specifier for " << UlamType::getUlamTypeEnumAsString(etype);
		msg << " type, within (), is a negative numeric constant expression: ";
		msg << bs;
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
		rtnuti = Nav;
		return false;
	      }

	    // keep m_unknownBitsizeSubtree in case of template (don't delete)
	    if(!m_state.setUTISizes(rtnuti, bs, ut->getArraySize())) //update UlamType, outputs errs
	      {
		rtnuti = Nav;
		return false;
	      }
	  }
	else
	  {
	    rtnuti = auti; //could be Hzy or Nav
	    return false;
	  }
      }

    bool rtnb = m_state.isComplete(rtnuti);  //repeat if bitsize is still unknown
    if(!rtnb)
      rtnuti = Hzy;
    return rtnb;
  } //resolveTypeBitsize

  void NodeTypeDescriptor::countNavHzyNoutiNodes(u32& ncnt, u32& hcnt, u32& nocnt)
  {
    Node::countNavHzyNoutiNodes(ncnt, hcnt, nocnt);
    if(m_unknownBitsizeSubtree)
      m_unknownBitsizeSubtree->countNavHzyNoutiNodes(ncnt, hcnt, nocnt);
  }

  bool NodeTypeDescriptor::assignClassArgValueInStubCopy()
  {
    return true;
  }

  EvalStatus NodeTypeDescriptor::eval()
  {
    assert(0);  //not in parse tree; part of Node's type
    return NORMAL;
  } //eval

} //end MFM
