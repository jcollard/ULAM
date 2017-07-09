#include "NodeBinaryOpEqual.h"
#include "NodeFunctionCall.h"
#include "NodeMemberSelect.h"
#include "SymbolFunctionName.h"
#include "SymbolFunction.h"
#include "CompilerState.h"

namespace MFM {

  NodeBinaryOpEqual::NodeBinaryOpEqual(Node * left, Node * right, CompilerState & state) : NodeBinaryOp(left,right,state)
  {
    Node::setStoreIntoAble(TBOOL_HAZY);
  }

  NodeBinaryOpEqual::NodeBinaryOpEqual(const NodeBinaryOpEqual& ref) : NodeBinaryOp(ref) {}

  NodeBinaryOpEqual::~NodeBinaryOpEqual(){}

  Node * NodeBinaryOpEqual::instantiate()
  {
    return new NodeBinaryOpEqual(*this);
  }

  const char * NodeBinaryOpEqual::getName()
  {
    return "=";
  }

  const std::string NodeBinaryOpEqual::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  const std::string NodeBinaryOpEqual::methodNameForCodeGen()
  {
    return "_Equal_Stub";
  }

  bool NodeBinaryOpEqual::checkSafeToCastTo(UTI unused, UTI& newType)
  {
    bool rtnOK = true;
    FORECAST scr = m_nodeRight->safeToCastTo(newType);
    if(scr != CAST_CLEAR)
      {
	ULAMTYPE etyp = m_state.getUlamTypeByIndex(newType)->getUlamTypeEnum();
	std::ostringstream msg;
	if(etyp == Bool)
	  msg << "Use a comparison operator";
	else
	  msg << "Use explicit cast";
	msg << " to convert "; // the real converting-message
	msg << m_state.getUlamTypeNameBriefByIndex(m_nodeRight->getNodeType()).c_str();
	msg << " to ";
	msg << m_state.getUlamTypeNameBriefByIndex(newType).c_str();
	msg << " for operator" << getName();
	if(scr == CAST_HAZY)
	  {
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), WAIT);
	    m_state.setGoAgain();
	    newType = Hzy;
	  }
	else
	  {
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	    newType = Nav;
	  }
	rtnOK = false;
      } //not safe
    else if(m_nodeRight->isExplicitReferenceCast())
      {
	std::ostringstream msg;
	msg << "Explicit Reference cast of assignment is invalid";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	newType = Nav;
	rtnOK = false;
      }
    return rtnOK;
  } //checkSafeToCastTo

  UTI NodeBinaryOpEqual::checkAndLabelType()
  {
    assert(m_nodeLeft && m_nodeRight);

    UTI leftType = m_nodeLeft->checkAndLabelType();
    UTI rightType = m_nodeRight->checkAndLabelType();

    if(!m_state.neitherNAVokUTItoContinue(leftType, rightType))
      {
	std::ostringstream msg;
	msg << "Assignment is invalid";
	msg << "; LHS: ";
	msg << m_state.getUlamTypeNameBriefByIndex(leftType);
	msg << "; RHS: ";
	msg << m_state.getUlamTypeNameBriefByIndex(rightType);

	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	setNodeType(Nav);
	return Nav;
      }

    if(!m_state.isComplete(leftType) || !m_state.isComplete(rightType))
      {
    	setNodeType(Hzy);
	m_state.setGoAgain(); //for compiler counts
    	return Hzy; //not quietly
      }

    TBOOL stor = checkStoreIntoAble();
    if(stor == TBOOL_FALSE)
      {
	setNodeType(Nav);
	return Nav;
      }
    else if(stor == TBOOL_HAZY)
      {
	setNodeType(Hzy);
	m_state.setGoAgain();
      }

    if(!NodeBinaryOp::checkNotVoidTypes(leftType, rightType, false))
      {
    	setNodeType(Nav);
    	return Nav;
      }

    if(m_nodeRight->isExplicitReferenceCast())
      {
	std::ostringstream msg;
	msg << "Explicit Reference cast of assignment is invalid";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	setNodeType(Nav);
	return Nav;
      }

    if(m_state.isScalar(leftType) ^ m_state.isScalar(rightType))
      {
	std::ostringstream msg;
	msg << "Incompatible (nonscalar) types: ";
	msg << m_state.getUlamTypeNameBriefByIndex(leftType).c_str();
	msg << " and ";
	msg << m_state.getUlamTypeNameBriefByIndex(rightType).c_str();
	msg << " used with binary operator" << getName();
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	setNodeType(Nav);
	return Nav;
      }

    UTI newType = leftType;
    UlamType * lut = m_state.getUlamTypeByIndex(leftType);
    //cast RHS if necessary and safe
    if(UlamType::compare(newType, rightType, m_state) != UTIC_SAME)
      {
	//different msg if try to assign non-class to a class type
	if((lut->getUlamTypeEnum() == Class) && (m_state.getUlamTypeByIndex(rightType)->getUlamTypeEnum() != Class) && !m_state.isAtom(rightType))
	  {
	    //try for operator overload first (e.g. (pre) +=,-=, (post) ++,-- )
	    Node * newnode = buildOperatorOverloadFuncCallNode(); //virtual
	    if(newnode)
	      {
		AssertBool swapOk = Node::exchangeNodeWithParent(newnode);
		assert(swapOk);

		m_nodeLeft = NULL; //recycle as memberselect
		m_nodeRight = NULL; //recycle as func call arg

		delete this; //suicide is painless..

		return newnode->checkAndLabelType();
	      }
	    else
	      {
		std::ostringstream msg;
		msg << "Incompatible class type ";
		msg << m_state.getUlamTypeNameBriefByIndex(leftType).c_str();
		msg << " and ";
		msg << m_state.getUlamTypeNameBriefByIndex(rightType).c_str();
		msg << " used with binary operator" << getName();
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
		newType = Nav; //error
	      }
	  }
	else
	  {
	    //for assignment to reference, cast to underlying type Sat Jun 25 09:01:07 2016
	    newType = m_state.getUlamTypeAsDeref(leftType); //init
	    if(checkSafeToCastTo(rightType, newType))
	      {
		if(!Node::makeCastingNode(m_nodeRight, newType, m_nodeRight))
		  newType = Nav; //error
		else
		  {
		    //for ulam compiler, cast to reference if necessary Sat Jun 25 09:08:37 2016
		    if(UlamType::compare(leftType, newType, m_state) != UTIC_SAME)
		      {
			newType = leftType;
			if(!Node::makeCastingNode(m_nodeRight, newType, m_nodeRight))
			  newType = Nav; //error
			//else not safe, newType changed
		      }
		  }
	      }
	    //else not safe, newType changed
	  }
      }
    else
      {
	if(lut->getUlamTypeEnum() == Class)
	  {
	    //try for operator overload first
	    Node * newnode = buildOperatorOverloadFuncCallNode(); //virtual
	    if(newnode)
	      {
		AssertBool swapOk = Node::exchangeNodeWithParent(newnode);
		assert(swapOk);

		m_nodeLeft = NULL; //recycle as memberselect
		m_nodeRight = NULL; //recycle as func call arg

		delete this; //suicide is painless..

		return newnode->checkAndLabelType();
	      }
	    //else not overloaded, use default
	  }
      }
    setNodeType(newType);
    return newType;
  } //checkAndLabelType

  //here, we check for func with matching argument when both sides the same Class type
  //so we can use the default struct equal if no overload defined.
  Node * NodeBinaryOpEqual::buildOperatorOverloadFuncCallNode()
  {
    UTI leftType = m_nodeLeft->getNodeType();
    UTI rightType = m_nodeRight->getNodeType();
    UlamType * rut = m_state.getUlamTypeByIndex(rightType);

    if((UlamType::compare(leftType, rightType, m_state) != UTIC_SAME) && (rut->getUlamClassType() == UC_NOTACLASS))
      return NodeBinaryOp::buildOperatorOverloadFuncCallNode(); //t41117,18,20,21

    Token identTok;
    TokenType opTokType = Token::getTokenTypeFromString(getName());
    assert(opTokType != TOK_LAST_ONE);
    Token opTok(opTokType, getNodeLocation(), 0);
    u32 opolId = Token::getOperatorOverloadFullNameId(opTok, &m_state);
    if(opolId == 0)
      {
	std::ostringstream msg;
	msg << "Overload for operator <" << getName();
	msg << "> is not supported as operand for class: ";
	msg << m_state.getUlamTypeNameBriefByIndex(leftType).c_str();
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	return NULL;
      }

    identTok.init(TOK_IDENTIFIER, getNodeLocation(), opolId);

    //may need to fall back to default struct equal when the same class (t41119)
    // when matching function not found; ref on rhs should also match non-ref arg if
    // ref arg not found (t41120)
    SymbolClass * csym = NULL;
    AssertBool isDefined = m_state.alreadyDefinedSymbolClass(leftType, csym);
    assert(isDefined);

    NodeBlockClass * memberClassNode = csym->getClassBlockNode();
    assert(memberClassNode);  //e.g. forgot the closing brace on quark definition

    assert(m_state.okUTItoContinue(memberClassNode->getNodeType()));

   //set up compiler state to use the member class block for symbol searches
    m_state.pushClassContextUsingMemberClassBlock(memberClassNode);

    Node * rtnNode = NULL;
    Symbol * fnsymptr = NULL;
    bool hazyKin = false; //unused

    if(m_state.isFuncIdInClassScope(opolId, fnsymptr, hazyKin))
      {
	// still need to pinpoint the SymbolFunction; match by Types rather than looking for safe casts.
	std::vector<UTI> pTypes;
	pTypes.push_back(rightType);

	SymbolFunction * funcSymbol = NULL;
	u32 numFuncs = ((SymbolFunctionName *) fnsymptr)->findMatchingFunctionStrictlyByTypes(pTypes, funcSymbol);
	if(numFuncs == 0 && m_state.isReference(rightType))
	  {
	    //try again with non-ref type (t41120)
	    UTI deref = m_state.getUlamTypeAsDeref(rightType);

	    std::vector<UTI> dTypes;
	    dTypes.push_back(deref);

	    numFuncs = ((SymbolFunctionName *) fnsymptr)->findMatchingFunctionStrictlyByTypes(dTypes, funcSymbol);
	  }

	if(numFuncs >= 1)
	  {
	    // ambiguous (>1) overload will produce an error later
	    //fill in func symbol during type labeling;
	    NodeFunctionCall * fcallNode = new NodeFunctionCall(identTok, NULL, m_state);
	    assert(fcallNode);
	    fcallNode->setNodeLocation(identTok.m_locator);

	    fcallNode->addArgument(m_nodeRight);

	    NodeMemberSelect * mselectNode = new NodeMemberSelect(m_nodeLeft, fcallNode, m_state);
	    assert(mselectNode);
	    mselectNode->setNodeLocation(identTok.m_locator);
	    rtnNode = mselectNode;
	  }
	//else use default struct equal
      }

    //clear up compiler state to no longer use the member class block for symbol searches
    m_state.popClassContext();

    //redo check and type labeling done by caller!!
    return rtnNode; //replace right node with new branch
  } //buildOperatorOverloadFuncCallNode

  TBOOL NodeBinaryOpEqual::checkStoreIntoAble()
  {
    TBOOL lstor = m_nodeLeft->getStoreIntoAble();
    if(lstor != TBOOL_TRUE)
      {
	UTI lt = m_nodeLeft->getNodeType();
	std::ostringstream msg;
	msg << "Invalid lefthand side of equals <" << m_nodeLeft->getName();
	msg << ">, type: " << m_state.getUlamTypeNameBriefByIndex(lt).c_str();
	if(lstor == TBOOL_HAZY)
	  MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), WAIT);
	else
	  MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
      }
    setStoreIntoAble(lstor);
    return lstor;
  } //checkStoreIntoAble

  bool NodeBinaryOpEqual::checkNotUnpackedArray()
  {
    bool rtnOK = true;
    UTI lt = m_nodeLeft->getNodeType();
    UTI rt = m_nodeRight->getNodeType();
    PACKFIT lpacked = m_state.determinePackable(lt);
    PACKFIT rpacked = m_state.determinePackable(rt);
    bool unpackedArrayLeft = !WritePacked(lpacked) && !m_state.isScalar(lt);
    bool unpackedArrayRight = !WritePacked(rpacked) && !m_state.isScalar(rt);

    if(unpackedArrayLeft || unpackedArrayRight)
      {
	if(unpackedArrayLeft)
	  {
	    std::ostringstream msg;
	    msg << "Lefthand side of equals requires UNPACKED array support <";
	    msg << m_nodeLeft->getName();
	    msg << ">, type: " << m_state.getUlamTypeNameBriefByIndex(lt).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	  }
	if(unpackedArrayRight)
	  {
	    std::ostringstream msg;
	    msg << "Righthand side of equals requires UNPACKED array support <";
	    msg << m_nodeRight->getName();
	    msg << ">, type: " << m_state.getUlamTypeNameBriefByIndex(rt).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	  }
	rtnOK = false;
      }
    return rtnOK;
  } //checkNotUnpackedArray

  UTI NodeBinaryOpEqual::calcNodeType(UTI lt, UTI rt)
  {
    m_state.abortShouldntGetHere();
    return Nav;
  }

  EvalStatus NodeBinaryOpEqual::eval()
  {
    assert(m_nodeLeft && m_nodeRight);

    UTI nuti = getNodeType();
    if(nuti == Nav)
      return ERROR;

    if(nuti == Hzy)
      return NOTREADY;

    evalNodeProlog(0); //new current frame pointer on node eval stack

    makeRoomForSlots(1); //always 1 slot for ptr

    EvalStatus evs = m_nodeLeft->evalToStoreInto();
    if(evs != NORMAL)
      {
	evalNodeEpilog();
	return evs;
      }

    u32 slot = makeRoomForNodeType(nuti);
    evs = m_nodeRight->eval(); //a Node Function Call here
    if(evs != NORMAL)
      {
	evalNodeEpilog();
	return evs;
      }

    //assigns rhs to lhs UV pointer (handles arrays);
    //also copy result UV to stack, -1 relative to current frame pointer
    if(slot)
      if(!doBinaryOperation(1, 2, slot))
	evs = ERROR;

    evalNodeEpilog();
    return evs;
  } //eval

  EvalStatus NodeBinaryOpEqual::evalToStoreInto()
  {
    UTI nuti = getNodeType();
    if(nuti == Nav)
      return ERROR;

    if(nuti == Hzy)
      return NOTREADY;

    evalNodeProlog(0);

    makeRoomForSlots(1); //always 1 slot for ptr
    EvalStatus evs = m_nodeLeft->evalToStoreInto();
    if(evs != NORMAL)
      {
	evalNodeEpilog();
	return evs;
      }

    UlamValue luvPtr = UlamValue::makePtr(1, EVALRETURN, nuti, m_state.determinePackable(nuti), m_state); //positive to current frame pointer

    Node::assignReturnValuePtrToStack(luvPtr);

    evalNodeEpilog();
    return NORMAL;
  } //evalToStoreInto

  bool NodeBinaryOpEqual::doBinaryOperation(s32 lslot, s32 rslot, u32 slots)
  {
    assert(slots);
    UTI nuti = getNodeType();
    UlamValue pluv = m_state.m_nodeEvalStack.loadUlamValuePtrFromSlot(lslot);
    UlamValue ruv;

    if(!pluv.isPtr())
      return false; //e.g. t41089 func call used on lhs (gen code ok)

    if(m_state.isScalar(nuti))
      {
	ruv = m_state.m_nodeEvalStack.loadUlamValueFromSlot(rslot); //immediate scalar
      }
    else
      {
	PACKFIT packed = m_state.determinePackable(nuti);
	if(WritePacked(packed))
	  {
	    ruv = m_state.m_nodeEvalStack.loadUlamValueFromSlot(rslot); //packed/PL array
	  }
	else
	  {
	    // unpacked array requires a ptr
	    ruv = UlamValue::makePtr(rslot, EVALRETURN, nuti, packed, m_state); //ptr
	  }
      }

    UTI ruti = ruv.getUlamValueTypeIdx();
    if(ruti == Nav || nuti == Nav)
      return false;

    if(ruti == Hzy || nuti == Hzy)
      return false;

    //before assert in CS, fails
    UTI luti = pluv.getPtrTargetType();
    if( m_state.isPtr(ruti) || (UlamType::compareForUlamValueAssignment(luti, ruti, m_state) == UTIC_SAME) || m_state.isAtom(luti) || m_state.isAtom(ruti))
      m_state.assignValue(pluv,ruv);
    else
      return false;

    //also copy result UV to stack, -1 relative to current frame pointer
    Node::assignReturnValueToStack(ruv);
    return true;
  } //dobinaryoperation

  bool NodeBinaryOpEqual::doBinaryOperationImmediate(s32 lslot, s32 rslot, u32 slots)
  {
    assert(slots == 1);
    UTI nuti = getNodeType();
    u32 len = m_state.getTotalBitSize(nuti);

    // 'pluv' is where the resulting sum needs to be stored
    UlamValue pluv = m_state.m_nodeEvalStack.loadUlamValuePtrFromSlot(lslot); //a Ptr
    assert(m_state.isPtr(pluv.getUlamValueTypeIdx()) && (UlamType::compare(pluv.getPtrTargetType(),nuti, m_state) == UTIC_SAME));

    assert(slots == 1);
    UlamValue luv = m_state.getPtrTarget(pluv);  //no eval!!
    UlamValue ruv = m_state.m_nodeEvalStack.loadUlamValueFromSlot(rslot); //immediate value

    if((luv.getUlamValueTypeIdx() == Nav) || (ruv.getUlamValueTypeIdx() == Nav))
      return false;

    if((luv.getUlamValueTypeIdx() == Hzy) || (ruv.getUlamValueTypeIdx() == Hzy))
      return false;

    UlamValue rtnUV;
    u32 wordsize = m_state.getTotalWordSize(nuti);
    if(wordsize == MAXBITSPERINT)
      {
	u32 ldata = luv.getDataFromAtom(pluv, m_state);
	u32 rdata = ruv.getImmediateData(len, m_state);
	rtnUV = makeImmediateBinaryOp(nuti, ldata, rdata, len);
      }
    else if(wordsize == MAXBITSPERLONG)
      {
	u64 ldata = luv.getDataLongFromAtom(pluv, m_state);
	u64 rdata = ruv.getImmediateDataLong(len);
	rtnUV = makeImmediateLongBinaryOp(nuti, ldata, rdata, len);
      }
    else
      m_state.abortGreaterThanMaxBitsPerLong();//e.g. 0

    if(rtnUV.getUlamValueTypeIdx() == Nav)
      return false;

    if(rtnUV.getUlamValueTypeIdx() == Hzy)
      return false;

    m_state.assignValue(pluv,rtnUV);

    //also copy result UV to stack, -1 relative to current frame pointer
    m_state.m_nodeEvalStack.storeUlamValueInSlot(rtnUV, -1);
    return true;
  } //dobinaryopImmediate

  bool NodeBinaryOpEqual::doBinaryOperationArray(s32 lslot, s32 rslot, u32 slots)
  {
    UlamValue rtnUV;
    UTI nuti = getNodeType();
    s32 arraysize = m_state.getArraySize(nuti); //could be zero length array
    s32 bitsize = m_state.getBitSize(nuti);
    UTI scalartypidx = m_state.getUlamTypeAsScalar(nuti);
    PACKFIT packRtn = m_state.determinePackable(nuti);

    if(WritePacked(packRtn))
      {
	//pack result too. (slot size known ahead of time)
	rtnUV = UlamValue::makeAtom(nuti); //accumulate result here
      }

    // 'pluv' is where the resulting sum needs to be stored
    UlamValue pluv = m_state.m_nodeEvalStack.loadUlamValuePtrFromSlot(lslot); //a Ptr
    assert(m_state.isPtr(pluv.getUlamValueTypeIdx()) && (UlamType::compare(pluv.getPtrTargetType(), nuti, m_state) == UTIC_SAME));

    // point to base array slots, packedness determines its 'pos'
    UlamValue lArrayPtr = pluv;
    UlamValue rArrayPtr = UlamValue::makePtr(rslot, EVALRETURN, nuti, packRtn, m_state);

    // to use incrementPtr(), 'pos' depends on packedness
    UlamValue lp = UlamValue::makeScalarPtr(lArrayPtr, m_state);
    UlamValue rp = UlamValue::makeScalarPtr(rArrayPtr, m_state);

    u32 navCount = 0;
    u32 hzyCount = 0;

    //make immediate result for each element inside loop
    for(s32 i = 0; i < arraysize; i++)
      {
	UlamValue luv = m_state.getPtrTarget(lp);
	UlamValue ruv = m_state.getPtrTarget(rp);

	u32 ldata = luv.getData(lp.getPtrPos(), bitsize); //'pos' doesn't vary for unpacked
	u32 rdata = ruv.getData(rp.getPtrPos(), bitsize); //'pos' doesn't vary for unpacked

	if(WritePacked(packRtn))
	  // use calc position where base [0] is furthest from the end.
	  appendBinaryOp(rtnUV, ldata, rdata, (BITSPERATOM-(bitsize * (arraysize - i))), bitsize);
	else
	  {
	    // else, unpacked array
	    rtnUV = makeImmediateBinaryOp(scalartypidx, ldata, rdata, bitsize);
	    if(rtnUV.getUlamValueTypeIdx() == Nav)
	      navCount++;
	    else if(rtnUV.getUlamValueTypeIdx() == Hzy)
	      hzyCount++;

	    // overwrite lhs copy with result UV
	    m_state.assignValue(lp, rtnUV);

	    //copy result UV to stack, -1 (1st array element deepest) relative to current frame pointer
	    m_state.m_nodeEvalStack.storeUlamValueInSlot(rtnUV, -slots + i);
	  }

	AssertBool isNextLeft = lp.incrementPtr(m_state);
	assert(isNextLeft);
	AssertBool isNextRight = rp.incrementPtr(m_state);
	assert(isNextRight);
      } //forloop

    if(navCount > 0)
      return false;

    if(hzyCount > 0)
      return false;

    if(WritePacked(packRtn))
      {
	m_state.assignValue(pluv, rtnUV); //overwrite lhs copy with result UV
	m_state.m_nodeEvalStack.storeUlamValueInSlot(rtnUV, -1); //store accumulated packed result
      }
    return true;
  } //dobinaryoparray

  UlamValue NodeBinaryOpEqual::makeImmediateBinaryOp(UTI type, u32 ldata, u32 rdata, u32 len)
  {
    m_state.abortShouldntGetHere(); //unused
    return UlamValue();
  }

  UlamValue NodeBinaryOpEqual::makeImmediateLongBinaryOp(UTI type, u64 ldata, u64 rdata, u32 len)
  {
    m_state.abortShouldntGetHere(); //unused
    return UlamValue();
  }

  void NodeBinaryOpEqual::appendBinaryOp(UlamValue& refUV, u32 ldata, u32 rdata, u32 pos, u32 len)
  {
    m_state.abortShouldntGetHere(); //unused
  }

  void NodeBinaryOpEqual::calcMaxDepth(u32& depth, u32& maxdepth, s32 base)
  {
    assert(m_nodeRight);
    m_nodeRight->calcMaxDepth(depth, maxdepth, base); //funccall?
    return; //work done by NodeStatements and NodeBlock
  }

  void NodeBinaryOpEqual::genCode(File * fp, UVPass& uvpass)
  {
    assert(m_nodeLeft && m_nodeRight);
    assert(m_state.m_currentObjSymbolsForCodeGen.empty());

    // generate rhs first; may update current object globals (e.g. function call)
    UVPass ruvpass;
    m_nodeRight->genCode(fp, ruvpass);

    // restore current object globals
    assert(m_state.m_currentObjSymbolsForCodeGen.empty()); //*************

    // lhs should be the new current object: node member select updates them,
    // but a plain NodeIdent does not!!!  because genCodeToStoreInto has been repurposed
    // to mean "don't read into a TmpVar" (e.g. by NodeCast).
    UVPass luvpass;
    m_nodeLeft->genCodeToStoreInto(fp, luvpass); //may update m_currentObjSymbol, m_currentSelfSymbol

    // current object globals should pertain to lhs for the write
    Node::genCodeWriteFromATmpVar(fp, luvpass, ruvpass); //uses rhs' tmpvar

    uvpass = ruvpass; //in case we're the rhs of an equals..

    assert(m_state.m_currentObjSymbolsForCodeGen.empty());
  } //genCode

} //end MFM
