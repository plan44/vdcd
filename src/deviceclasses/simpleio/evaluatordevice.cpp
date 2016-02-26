//
//  Copyright (c) 2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 6

#include "evaluatordevice.hpp"

#include "buttonbehaviour.hpp"
#include "binaryinputbehaviour.hpp"

using namespace p44;


EvaluatorDevice::EvaluatorDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  evaluatorType(evaluator_unknown),
  currentState(undefined),
  valueParseTicket(0)
{
  #warning "%%% test only"
  valueDefs = "lux:E5FAAC1FA8905381803BA01AD1C8174C00_S0, test:C0B813EA37CC5161C02D591A7B15627B00_S0";
  onCondition = "lux<42 & test>4+3*2";
  offCondition = "lux>44 | test<3*(2+1)";

  // Config is:
  //  <id>:<behaviour mode>
  //  - where id must be an unique string from which the dSUID will be derived
  size_t i = aDeviceConfig.find(":");
  string name = aDeviceConfig;
  if (i!=string::npos) {
    name = aDeviceConfig.substr(0,i);
    string mode = aDeviceConfig.substr(i+1,string::npos);
    if (mode=="rocker")
      evaluatorType = evaluator_rocker;
    else if (mode=="input")
      evaluatorType = evaluator_input;
    else {
      LOG(LOG_ERR, "unknown evaluator type: %s", mode.c_str());
    }
  }
  // assign name for showing on console and for creating dSUID from
  evaluatorID = name;
  // create I/O
  if (evaluatorType==evaluator_rocker) {
    // Simulate Two-way Rocker Button device
    // - defaults to black (generic button)
    primaryGroup = group_black_joker;
    // - standard device settings without scene table
    installSettings();
    // - create down button (index 0)
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false, 1, true); // counterpart up-button has buttonIndex 1, fixed mode
    b->setGroup(group_black_joker); // pre-configure for app button
    addBehaviour(b);
    // - create up button (index 1)
    b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false, 0, true); // counterpart down-button has buttonIndex 0, fixed mode
    b->setGroup(group_black_joker); // pre-configure for app button
    addBehaviour(b);
  }
  else if (evaluatorType==evaluator_input) {
    // Standard device settings without scene table
    primaryGroup = group_black_joker;
    installSettings();
    // - create one binary input
    BinaryInputBehaviourPtr b = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    b->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
    addBehaviour(b);
  }
  deriveDsUid();
}


EvaluatorDevice::~EvaluatorDevice()
{
  forgetValueDefs();
}



string EvaluatorDevice::modelName()
{
  switch (evaluatorType) {
    case evaluator_rocker: return "evaluated up/down button";
    case evaluator_input: return "evaluated input";
    default: break;
  }
  return "";
}



bool EvaluatorDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("evaluator", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


void EvaluatorDevice::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  // try connecting to values now. In case not all values are found, this will be re-executed later
  parseValueDefs();
  // done
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}




void EvaluatorDevice::forgetValueDefs()
{
  for (ValueSourcesMap::iterator pos = valueMap.begin(); pos!=valueMap.end(); ++pos) {
    pos->second->removeSourceListener(this);
  }
  valueMap.clear();
}



#define REPARSE_DELAY (30*Second)

void EvaluatorDevice::parseValueDefs()
{
  forgetValueDefs(); // forget previous mappings
  // syntax:
  //  <valuealias>:<valuesourceid> [, <valuealias>:valuesourceid> ...]
  bool foundall = true;
  size_t i = 0;
  while(i<valueDefs.size()) {
    size_t e = valueDefs.find(":", i);
    if (e!=string::npos) {
      string valuealias = valueDefs.substr(i,e-i);
      i = e+1;
      size_t e2 = valueDefs.find_first_of(", \t", i);
      if (e2==string::npos) e2 = valueDefs.size();
      string valuesourceid = valueDefs.substr(i,e2-i);
      // search source
      ValueSource *vs = getDeviceContainer().getValueSourceById(valuesourceid);
      if (vs) {
        // value source exists
        // - add myself as listener
        vs->addSourceListener(boost::bind(&EvaluatorDevice::dependentValueNotification, this, _1, _2), this);
        // - add source to my map
        valueMap[valuealias] = vs;
        AFOCUSLOG("Parsed value definition: %s : %s", valuealias.c_str(), vs->getSourceName().c_str());
      }
      else {
        ALOG(LOG_WARNING, "value source '%s' currently not found", valuesourceid.c_str());
        foundall = false;
      }
      // skip delimiters
      i = valueDefs.find_first_not_of(", \t", e2);
      if (i==string::npos) i = valueDefs.size();
    }
    else {
      ALOG(LOG_ERR, "missing ':' in value definition");
      break;
    }
  }
  if (!foundall) {
    // schedule a re-parse later
    MainLoop::currentMainLoop().executeTicketOnce(valueParseTicket, boost::bind(&EvaluatorDevice::parseValueDefs, this), REPARSE_DELAY);
  }
}


void EvaluatorDevice::dependentValueNotification(ValueSource &aValueSource, ValueListenerEvent aEvent)
{
  if (aEvent==valueevent_removed) {
    // a value has been removed, update my map
    parseValueDefs();
  }
  else {
    ALOG(LOG_INFO, "value source '%s' reports value %f", aValueSource.getSourceName().c_str(), aValueSource.getSourceValue());
    // evaluate state and report it
    if (currentState!=yes) {
      // off or unknown: check for switching on
      if (evaluateBoolean(onCondition)==yes) currentState=yes;
    }
    if (currentState!=no) {
      // on or unknown: check for switching off
      if (evaluateBoolean(offCondition)==yes) currentState=no;
    }
    if (currentState!=undefined) {
      // report it
      switch (evaluatorType) {
        case evaluator_input : {
          BinaryInputBehaviourPtr b = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[0]);
          if (b) {
            b->updateInputState(currentState==yes);
          }
          break;
        }
        case evaluator_rocker : {
          // virtually click up or down button
          ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[currentState==no ? 0 : 1]);
          if (b) {
            b->sendClick(ct_tip_1x);
          }
          break;
        }
        default: break;
      }
    }
  }
}


Tristate EvaluatorDevice::evaluateBoolean(string aExpression)
{
  ErrorPtr err;
  double v;
  const char *p = aExpression.c_str();
  AFOCUSLOG("----- Starting expression evaluation: '%s'", p);
  err = evaluateExpression(p, v, 0);
  if (Error::isOK(err)) {
    // evaluation successful
    AFOCUSLOG("===== expression result: '%s' = %f = %s", aExpression.c_str(), v, v>0 ? "true" : "false");
    return v>0 ? yes : no;
  }
  else {
    ALOG(LOG_INFO,"Expression '%s' evaluation error: %s", aExpression.c_str(), err->description().c_str());
    return undefined;
  }
}




ErrorPtr EvaluatorDevice::evaluateTerm(const char * &aText, double &aValue)
{
  const char *a = aText;
  // a term can be
  // - a variable reference, a literal number or a parantesis containing an expression
  while (*aText==' ' || *aText=='\t') aText++; // skip whitespace
  // extract var name or number
  double v = 0;
  const char *e = aText;
  while (*e && (isalnum(*e) || *e=='.' || *e=='_')) e++;
  if (e==aText) return TextError::err("missing term");
  // must be simple term
  string term;
  term.assign(aText, e-aText);
  aText = e; // advance cursor
  // skip trailing whitespace
  while (*aText==' ' || *aText=='\t') aText++; // skip whitespace
  // decode term
  if (isalpha(term[0])) {
    // must be a variable
    ValueSourcesMap::iterator pos = valueMap.find(term);
    if (pos==valueMap.end()) {
      return TextError::err("Undefined variable '%s' -> undefined result", term.c_str());
    }
    // value found, get it
    if (pos->second->getSourceAge()==Never) {
      // no value known yet
      return TextError::err("Variable '%s' has no known value yet -> undefined result", term.c_str());
    }
    else {
      v = pos->second->getSourceValue();
    }
  }
  else {
    // must be a numeric literal
    if (sscanf(term.c_str(), "%lf", &v)!=1) {
      return TextError::err("'%s' is not a valid number", term.c_str());
    }
  }
  // valid term
  AFOCUSLOG("Term '%.*s' evaluation result: %lf", (int)(aText-a), a, v);
  aValue = v;
  return ErrorPtr();
}


// operations with precedence
typedef enum {
  op_none     = 0x06,
  op_not      = 0x16,
  op_multiply = 0x25,
  op_divide   = 0x35,
  op_add      = 0x44,
  op_subtract = 0x54,
  op_equal    = 0x63,
  op_notequal = 0x73,
  op_less     = 0x83,
  op_greater  = 0x93,
  op_leq      = 0xA3,
  op_geq      = 0xB3,
  op_and      = 0xC2,
  op_or       = 0xD2,
  opmask_precedence = 0x0F
} Operations;


// a + 3 * 4

static Operations parseOperator(const char * &aText)
{
  while (*aText==' ' || *aText=='\t') aText++; // skip whitespace
  // check for operator
  Operations op = op_none;
  switch (*aText++) {
    case '*': op = op_multiply; break;
    case '/': op = op_divide; break;
    case '+': op = op_add; break;
    case '-': op = op_subtract; break;
    case '&': op = op_and; break;
    case '|': op = op_or; break;
    case '=': op = op_equal; break;
    case '<': {
      if (*aText=='=') {
        aText++; op = op_leq; break;
      }
      else if (*aText=='>') {
        aText++; op = op_notequal; break;
      }
      op = op_less; break;
    }
    case '>': {
      if (*aText=='=') {
        aText++; op = op_geq; break;
      }
      op = op_greater; break;
    }
    case '!': {
      if (*aText=='=') {
        aText++; op = op_notequal; break;
      }
      op = op_not; break;
      break;
    }
    default: --aText; // no expression char
  }
  while (*aText==' ' || *aText=='\t') aText++; // skip whitespace
  return op;
}




ErrorPtr EvaluatorDevice::evaluateExpression(const char * &aText, double &aValue, int aPrecedence)
{
  const char *a = aText;
  ErrorPtr err;
  double result = 0;
  // check for optional unary op
  Operations unaryop = parseOperator(aText);
  if (unaryop!=op_none) {
    if (unaryop!=op_subtract && unaryop!=op_not) {
      return TextError::err("invalid unary operator");
    }
  }
  // evaluate term
  // - check for paranthesis term
  if (*aText=='(') {
    // term is expression in paranthesis
    aText++;
    ErrorPtr err = evaluateExpression(aText, result, 0);
    if (Error::isOK(err)) {
      if (*aText!=')') {
        return TextError::err("Missing ')'");
      }
      aText++;
    }
  }
  else {
    // must be simple term
    err = evaluateTerm(aText, result);
    if (!Error::isOK(err)) return err;
  }
  // apply unary ops if any
  switch (unaryop) {
    case op_not : result = result > 0 ? 0 : 1; break;
    case op_subtract : result = -result; break;
    default: break;
  }
  while (*aText) {
    // now check for operator and precedence
    const char *optext = aText;
    Operations binaryop = parseOperator(optext);
    int precedence = binaryop & opmask_precedence;
    // end parsing here if end of text reached or operator has a lower or same precedence as the passed in precedence
    if (*optext==0 || *optext==')' || precedence<=aPrecedence) {
      // what we have so far is the result
      break;
    }
    // must parse right side of operator as subexpression
    aText = optext; // advance past operator
    double rightside;
    err = evaluateExpression(aText, rightside, precedence);
    if (!Error::isOK(err)) return err;
    // apply the operation between leftside and rightside
    switch (binaryop) {
      case op_not: {
        return TextError::err("NOT operator not allowed here");
      }
      case op_divide:
        if (rightside==0) return TextError::err("division by zero");
        result = result/rightside;
        break;
      case op_multiply: result = result*rightside; break;
      case op_add: result = result+rightside; break;
      case op_subtract: result = result-rightside; break;
      case op_equal: result = result==rightside; break;
      case op_notequal: result = result!=rightside; break;
      case op_less: result = result < rightside; break;
      case op_greater: result = result > rightside; break;
      case op_leq: result = result <= rightside; break;
      case op_geq: result = result >= rightside; break;
      case op_and: result = result && rightside; break;
      case op_or: result = result || rightside; break;
      default: break;
    }
    AFOCUSLOG("Intermediate expression '%.*s' evaluation result: %lf", (int)(aText-a), a, result);
  }
  // done
  aValue = result;
  return ErrorPtr();
}



void EvaluatorDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::evaluatorID
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::" + evaluatorID;
  dSUID.setNameInSpace(s, vdcNamespace);
}


string EvaluatorDevice::description()
{
  string s = inherited::description();
  if (evaluatorType==evaluator_rocker)
    string_format_append(s, "\n- evaluation controls simulated 2-way-rocker button");
  if (evaluatorType==evaluator_input)
    string_format_append(s, "\n- evaluation controls binary input");
  return s;
}
