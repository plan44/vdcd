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
#define FOCUSLOGLEVEL 7

#include "evaluatordevice.hpp"

#if ENABLE_EVALUATORS

#include "evaluatorvdc.hpp"

#include "buttonbehaviour.hpp"
#include "binaryinputbehaviour.hpp"

using namespace p44;


EvaluatorDevice::EvaluatorDevice(EvaluatorVdc *aVdcP, const string &aEvaluatorID, const string &aEvaluatorConfig) :
  inherited((Vdc *)aVdcP),
  evaluatorDeviceRowID(0),
  evaluatorID(aEvaluatorID),
  evaluatorType(evaluator_unknown),
  currentState(undefined),
  conditionMetSince(Never),
  onConditionMet(false),
  evaluateTicket(0),
  valueParseTicket(0)
{
  // Config is:
  //  <behaviour mode>
  if (aEvaluatorConfig=="rocker")
    evaluatorType = evaluator_rocker;
  else if (aEvaluatorConfig=="input")
    evaluatorType = evaluator_input;
  else {
    LOG(LOG_ERR, "unknown evaluator type: %s", aEvaluatorConfig.c_str());
  }
  // install our specific settings
  installSettings(DeviceSettingsPtr(new EvaluatorDeviceSettings(*this)));
  // create "inputs" that will deliver the evaluator's result
  if (evaluatorType==evaluator_rocker) {
    // Simulate Two-way Rocker Button device
    // - defaults to black (generic button)
    primaryGroup = group_black_joker;
    // - create down button (index 0)
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false, 1, true); // counterpart up-button has buttonIndex 1, fixed mode
    b->setHardwareName("off condition met");
    b->setGroup(group_black_joker); // pre-configure for app button
    addBehaviour(b);
    // - create up button (index 1)
    b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_2way, buttonElement_up, false, 0, true); // counterpart down-button has buttonIndex 0, fixed mode
    b->setHardwareName("on condition met");
    b->setGroup(group_black_joker); // pre-configure for app button
    addBehaviour(b);
  }
  else if (evaluatorType==evaluator_input) {
    // Standard device settings without scene table
    primaryGroup = group_black_joker;
    // - create one binary input
    BinaryInputBehaviourPtr b = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    b->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
    b->setHardwareName("evaluation result");
    addBehaviour(b);
  }
  deriveDsUid();
}


EvaluatorDevice::~EvaluatorDevice()
{
  forgetValueDefs();
}


EvaluatorVdc &EvaluatorDevice::getEvaluatorVdc()
{
  return *(static_cast<EvaluatorVdc *>(vdcP));
}


void EvaluatorDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // clear learn-in data from DB
  if (evaluatorDeviceRowID) {
    getEvaluatorVdc().db.executef("DELETE FROM evaluators WHERE rowid=%d", evaluatorDeviceRowID);
  }
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
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



ErrorPtr EvaluatorDevice::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  if (aMethod=="x-p44-checkEvaluator") {
    // Check the evaluator
    ApiValuePtr checkResult = aRequest->newApiValue();
    checkResult->setType(apivalue_object);
    // - value defs
    parseValueDefs(); // reparse
    ApiValuePtr valueDefs = checkResult->newObject();
    for (ValueSourcesMap::iterator pos = valueMap.begin(); pos!=valueMap.end(); ++pos) {
      ApiValuePtr val = valueDefs->newObject();
      MLMicroSeconds lastupdate = pos->second->getSourceLastUpdate();
      val->add("description", val->newString(pos->second->getSourceName()));
      if (lastupdate==Never) {
        val->add("age", val->newNull());
        val->add("value", val->newNull());
      }
      else {
        val->add("age", val->newDouble((double)(MainLoop::now()-lastupdate)/Second));
        val->add("value", val->newDouble(pos->second->getSourceValue()));
      }
      valueDefs->add(pos->first,val); // variable name
    }
    checkResult->add("valueDefs", valueDefs);
    // Conditions
    ApiValuePtr cond;
    double v;
    ErrorPtr err;
    // - on condition
    cond = checkResult->newObject();
    err = evaluateDouble(evaluatorSettings()->onCondition, v);
    if (Error::isOK(err)) {
      cond->add("result", cond->newDouble(v));
    }
    else {
      cond->add("error", cond->newString(err->getErrorMessage()));
    }
    checkResult->add("onCondition", cond);
    // - off condition
    cond = checkResult->newObject();
    err = evaluateDouble(evaluatorSettings()->offCondition, v);
    if (Error::isOK(err)) {
      cond->add("result", cond->newDouble(v));
    }
    else {
      cond->add("error", cond->newString(err->getErrorMessage()));
    }
    checkResult->add("offCondition", cond);
    // return the result
    aRequest->sendResult(checkResult);
    return ErrorPtr();
  }
  else {
    return inherited::handleMethod(aRequest, aMethod, aParams);
  }
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
  MainLoop::currentMainLoop().cancelExecutionTicket(valueParseTicket);
  forgetValueDefs(); // forget previous mappings
  string &valueDefs = evaluatorSettings()->valueDefs;
  // syntax:
  //  <valuealias>:<valuesourceid> [, <valuealias>:valuesourceid> ...]
  ALOG(LOG_INFO, "Parsing variable definitions");
  bool foundall = true;
  size_t i = 0;
  while(i<valueDefs.size()) {
    size_t e = valueDefs.find(":", i);
    if (e!=string::npos) {
      string valuealias = valueDefs.substr(i,e-i);
      i = e+1;
      size_t e2 = valueDefs.find_first_of(", \t\n\r", i);
      if (e2==string::npos) e2 = valueDefs.size();
      string valuesourceid = valueDefs.substr(i,e2-i);
      // search source
      ValueSource *vs = getVdc().getValueSourceById(valuesourceid);
      if (vs) {
        // value source exists
        // - add myself as listener
        vs->addSourceListener(boost::bind(&EvaluatorDevice::dependentValueNotification, this, _1, _2), this);
        // - add source to my map
        valueMap[valuealias] = vs;
        ALOG(LOG_INFO, "- Variable '%s' connected to source '%s'", valuealias.c_str(), vs->getSourceName().c_str());
      }
      else {
        ALOG(LOG_WARNING, "Value source id '%s' not found -> variable '%s' currently undefined", valuesourceid.c_str(), valuealias.c_str());
        foundall = false;
      }
      // skip delimiters
      i = valueDefs.find_first_not_of(", \t\n\r", e2);
      if (i==string::npos) i = valueDefs.size();
    }
    else {
      ALOG(LOG_ERR, "missing ':' in value definition");
      break;
    }
  }
  if (!foundall) {
    // schedule a re-parse later
    valueParseTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&EvaluatorDevice::parseValueDefs, this), REPARSE_DELAY);
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
    evaluateConditions();
  }
}


void EvaluatorDevice::evaluateConditions()
{
  // evaluate state and report it
  Tristate prevState = currentState;
  bool decisionMade = false;
  MLMicroSeconds now = MainLoop::currentMainLoop().now();
  MainLoop::currentMainLoop().cancelExecutionTicket(evaluateTicket);
  if (!decisionMade && currentState!=yes) {
    // off or unknown: check for switching on
    Tristate on = evaluateBoolean(evaluatorSettings()->onCondition);
    ALOG(LOG_INFO, "onCondition '%s' evaluates to %s", evaluatorSettings()->onCondition.c_str(), on==undefined ? "<undefined>" : (on==yes ? "true -> switching ON" : "false"));
    if (on!=yes) {
      // not met now -> reset if we are currently timing this condition
      if (onConditionMet) conditionMetSince = Never;
    }
    else {
      if (!onConditionMet || conditionMetSince==Never) {
        // we see this condition newly met now
        onConditionMet = true; // seen ON condition met
        conditionMetSince = now;
      }
      // check timing
      MLMicroSeconds metAt = conditionMetSince+evaluatorSettings()->minOnTime;
      if (now>=metAt) {
        // condition met long enough
        currentState = yes;
        decisionMade = true;
      }
      else {
        // condition not met long enough yet, need to re-check later
        ALOG(LOG_INFO, "- condition not yet met long enough -> must remain stable another %.2f seconds", (double)(metAt-now)/Second);
        evaluateTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&EvaluatorDevice::evaluateConditions, this), metAt);
        return;
      }
    }
  }
  if (!decisionMade && currentState!=no) {
    // on or unknown: check for switching off
    Tristate off = evaluateBoolean(evaluatorSettings()->offCondition);
    ALOG(LOG_INFO, "offCondition '%s' evaluates to %s", evaluatorSettings()->offCondition.c_str(), off==undefined ? "<undefined>" : (off==yes ? "true -> switching OFF" : "false"));
    if (off!=yes) {
      // not met now -> reset if we are currently timing this condition
      if (!onConditionMet) conditionMetSince = Never;
    }
    else {
      if (onConditionMet || conditionMetSince==Never) {
        // we see this condition newly met now
        onConditionMet = false; // seen OFF condition met
        conditionMetSince = now;
      }
      // check timing
      MLMicroSeconds metAt = conditionMetSince+evaluatorSettings()->minOffTime;
      if (now>=metAt) {
        // condition met long enough
        currentState = no;
        decisionMade = true;
      }
      else {
        // condition not met long enough yet, need to re-check later
        ALOG(LOG_INFO, "- condition not yet met long enough -> must remain stable another %.2f seconds", (double)(metAt-now)/Second);
        evaluateTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&EvaluatorDevice::evaluateConditions, this), metAt);
        return;
      }
    }
  }
  if (decisionMade && currentState!=undefined) {
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
        if (currentState!=prevState) {
          // virtually click up or down button
          ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[currentState==no ? 0 : 1]);
          if (b) {
            b->sendClick(ct_tip_1x);
          }
        }
        break;
      }
      default: break;
    }
  }
}


Tristate EvaluatorDevice::evaluateBoolean(string aExpression)
{
  AFOCUSLOG("----- Starting expression evaluation: '%s'", aExpression.c_str());
  double v = 0;
  ErrorPtr err = evaluateDouble(aExpression, v);
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


ErrorPtr EvaluatorDevice::evaluateDouble(string &aExpression, double &aResult)
{
  const char *p = aExpression.c_str();
  return evaluateExpression(p, aResult, 0);
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
      return TextError::err("Undefined variable '%s'", term.c_str());
    }
    // value found, get it
    if (pos->second->getSourceLastUpdate()==Never) {
      // no value known yet
      return TextError::err("Variable '%s' has no known value yet", term.c_str());
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
  string s = vdcP->vdcInstanceIdentifier();
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

#pragma mark - property access

enum {
  valueDefs_key,
  onCondition_key,
  offCondition_key,
  minOnTime_key,
  minOffTime_key,
  numProperties
};

static char evaluatorDevice_key;


int EvaluatorDevice::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  // Note: only add my own count when accessing root level properties!!
  if (!aParentDescriptor) {
    // Accessing properties at the Device (root) level, add mine
    return inherited::numProps(aDomain, aParentDescriptor)+numProperties;
  }
  // just return base class' count
  return inherited::numProps(aDomain, aParentDescriptor);
}


PropertyDescriptorPtr EvaluatorDevice::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numProperties] = {
    { "x-p44-valueDefs", apivalue_string, valueDefs_key, OKEY(evaluatorDevice_key) },
    { "x-p44-onCondition", apivalue_string, onCondition_key, OKEY(evaluatorDevice_key) },
    { "x-p44-offCondition", apivalue_string, offCondition_key, OKEY(evaluatorDevice_key) },
    { "x-p44-minOnTime", apivalue_double, minOnTime_key, OKEY(evaluatorDevice_key) },
    { "x-p44-minOffTime", apivalue_double, minOffTime_key, OKEY(evaluatorDevice_key) },
  };
  if (!aParentDescriptor) {
    // root level - accessing properties on the Device level
    int n = inherited::numProps(aDomain, aParentDescriptor);
    if (aPropIndex<n)
      return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
    aPropIndex -= n; // rebase to 0 for my own first property
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
  }
  else {
    // other level
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  }
}


// access to all fields
bool EvaluatorDevice::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(evaluatorDevice_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case valueDefs_key: aPropValue->setStringValue(evaluatorSettings()->valueDefs); return true;
        case onCondition_key: aPropValue->setStringValue(evaluatorSettings()->onCondition); return true;
        case offCondition_key: aPropValue->setStringValue(evaluatorSettings()->offCondition); return true;
        case minOnTime_key: aPropValue->setDoubleValue((double)(evaluatorSettings()->minOnTime)/Second); return true;
        case minOffTime_key: aPropValue->setDoubleValue((double)(evaluatorSettings()->minOffTime)/Second); return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case valueDefs_key:
          if (evaluatorSettings()->setPVar(evaluatorSettings()->valueDefs, aPropValue->stringValue()))
            parseValueDefs(); // changed valueDefs, re-parse them
          return true;
        case onCondition_key:
          if (evaluatorSettings()->setPVar(evaluatorSettings()->onCondition, aPropValue->stringValue()))
            evaluateConditions();  // changed conditions, re-evaluate output
          return true;
        case offCondition_key:
          if (evaluatorSettings()->setPVar(evaluatorSettings()->offCondition, aPropValue->stringValue()))
            evaluateConditions();  // changed conditions, re-evaluate output
          return true;
        case minOnTime_key:
          evaluatorSettings()->setPVar(evaluatorSettings()->minOnTime, (MLMicroSeconds)(aPropValue->doubleValue()*Second));
          return true;
        case minOffTime_key:
          evaluatorSettings()->setPVar(evaluatorSettings()->minOffTime, (MLMicroSeconds)(aPropValue->doubleValue()*Second));
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - settings


EvaluatorDeviceSettings::EvaluatorDeviceSettings(Device &aDevice) :
  inherited(aDevice),
  minOnTime(0), // trigger immediately
  minOffTime(0) // trigger immediately
{
}



const char *EvaluatorDeviceSettings::tableName()
{
  return "EvaluatorDeviceSettings";
}


// data field definitions

static const size_t numFields = 5;

size_t EvaluatorDeviceSettings::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *EvaluatorDeviceSettings::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "valueDefs", SQLITE_TEXT },
    { "onCondition", SQLITE_TEXT },
    { "offCondition", SQLITE_TEXT },
    { "minOnTime", SQLITE_INTEGER },
    { "minOffTime", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void EvaluatorDeviceSettings::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the field values
  valueDefs.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
  onCondition.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
  offCondition.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
  aRow->getCastedIfNotNull<MLMicroSeconds, long long int>(aIndex++, minOnTime);
  aRow->getCastedIfNotNull<MLMicroSeconds, long long int>(aIndex++, minOffTime);
}


// bind values to passed statement
void EvaluatorDeviceSettings::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, valueDefs.c_str(), false); // c_str() ist not static in general -> do not rely on it (even if static here)
  aStatement.bind(aIndex++, onCondition.c_str(), false); // c_str() ist not static in general -> do not rely on it (even if static here)
  aStatement.bind(aIndex++, offCondition.c_str(), false); // c_str() ist not static in general -> do not rely on it (even if static here)
  aStatement.bind(aIndex++, (long long int)minOnTime);
  aStatement.bind(aIndex++, (long long int)minOffTime);
}



#endif // ENABLE_EVALUATORS
