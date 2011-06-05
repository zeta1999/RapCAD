/*
 *   RapCAD - Rapid prototyping CAD IDE (www.rapcad.org)
 *   Copyright (C) 2010-2011 Giles Bathgate
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "evaluator.h"
#include "vectorvalue.h"
#include "rangevalue.h"

#include "module/echomodule.h"
#include "module/cubemodule.h"
#include "module/squaremodule.h"
#include "module/cylindermodule.h"
#include "module/circlemodule.h"
#include "module/polyhedronmodule.h"
#include "module/polylinemodule.h"
#include "module/beziersurfacemodule.h"

#include "module/differencemodule.h"
#include "module/unionmodule.h"
#include "module/groupmodule.h"
#include "module/intersectionmodule.h"
#include "module/translatemodule.h"
#include "module/symmetricdifferencemodule.h"
#include "module/minkowskimodule.h"
#include "module/glidemodule.h"
#include "module/linearextrudemodule.h"
#include "module/hullmodule.h"
#include "module/rotatemodule.h"
#include "module/mirrormodule.h"
#include "module/scalemodule.h"
#include "module/shearmodule.h"
#include "module/spheremodule.h"
#include "module/childmodule.h"
#include "module/boundsmodule.h"
#include "module/subdivisionmodule.h"
#include "module/offsetmodule.h"

#include "unionnode.h"

Evaluator::Evaluator(QTextStream& s) : output(s)
{
	context=NULL;
	rootNode=NULL;
}

Evaluator::~Evaluator()
{
}

QList<Declaration*> Evaluator::builtins;

/**
  Add the builtins to a static container so that they
  can be reused in subsequent evaluators.
*/
void Evaluator::initBuiltins(Script* sc)
{
	if(Evaluator::builtins.empty()) {
		Evaluator::builtins.append(new EchoModule(output));
		Evaluator::builtins.append(new CubeModule());
		Evaluator::builtins.append(new SquareModule());
		Evaluator::builtins.append(new CylinderModule());
		Evaluator::builtins.append(new CircleModule());
		Evaluator::builtins.append(new PolyhedronModule());
		Evaluator::builtins.append(new PolylineModule());
		Evaluator::builtins.append(new BezierSurfaceModule());
		Evaluator::builtins.append(new DifferenceModule());
		Evaluator::builtins.append(new UnionModule());
		Evaluator::builtins.append(new GroupModule());
		Evaluator::builtins.append(new IntersectionModule());
		Evaluator::builtins.append(new TranslateModule());
		Evaluator::builtins.append(new SymmetricDifferenceModule());
		Evaluator::builtins.append(new MinkowskiModule());
		Evaluator::builtins.append(new GlideModule());
		Evaluator::builtins.append(new HullModule());
		Evaluator::builtins.append(new LinearExtrudeModule());
		Evaluator::builtins.append(new RotateModule());
		Evaluator::builtins.append(new MirrorModule());
		Evaluator::builtins.append(new ScaleModule());
		Evaluator::builtins.append(new ShearModule());
		Evaluator::builtins.append(new SphereModule());
		Evaluator::builtins.append(new ChildModule());
		Evaluator::builtins.append(new BoundsModule());
		Evaluator::builtins.append(new SubDivisionModule());
		Evaluator::builtins.append(new OffsetModule());
	}
	foreach(Declaration* d,Evaluator::builtins)
		sc->addDeclaration(d);
}

void Evaluator::startContext(Scope* scp)
{
	Context* parent = context;
	context = new Context(output);
	context->parent = parent;
	context->currentScope=scp;
	contextStack.push(context);
}

void Evaluator::finishContext()
{
	contextStack.pop();
	context=contextStack.top();
}

void Evaluator::visit(ModuleScope* scp)
{
	QList<Value*> arguments = context->arguments;
	QList<Value*> parameters = context->parameters;
	QList<Node*> childnodes = context->inputNodes;

	startContext(scp);

	context->setArguments(arguments,parameters);
	context->inputNodes=childnodes;

	foreach(Declaration* d, scp->getDeclarations()) {
		d->accept(*this);
	}

	if(context->returnValue)
		output << "Warning: return statement not valid inside module scope.\n";

	//"pop" our child nodes.
	childnodes=context->currentNodes;
	finishContext();

	Node* n=createUnion(childnodes);
	context->currentNodes.append(n);
}

void Evaluator::visit(Instance* inst)
{
	QString name = inst->getName();

	QList <Statement*> stmts = inst->getChildren();
	if(stmts.size()>0) {
		startContext(context->currentScope);

		foreach(Statement* s,stmts) {
			s->accept(*this);
		}
		//"pop" our child nodes.
		QList<Node*> childnodes=context->currentNodes;
		finishContext();
		context->inputNodes=childnodes;
	}

	Module* mod = context->lookupModule(name);
	if(mod) {
		foreach(Argument* arg, inst->getArguments())
			arg->accept(*this);

		foreach(Parameter* p, mod->getParameters())
			p->accept(*this);

		Scope* scp = mod->getScope();
		if(scp) {
			scp->accept(*this);
		} else {
			Node* node=mod->evaluate(context,context->inputNodes);
			if(node)
				context->currentNodes.append(node);
		}

		context->arguments.clear();
		context->parameters.clear();
	} else {
		output << "Warning: cannot find module '" << name << "'.\n";
	}
}

void Evaluator::visit(Module* mod)
{
	context->addModule(mod);
}

void Evaluator::visit(Function* func)
{
	context->addFunction(func);
}

void Evaluator::visit(FunctionScope* scp)
{
	QList<Value*> arguments = context->arguments;
	QList<Value*> parameters = context->parameters;

	startContext(scp);

	context->setArguments(arguments,parameters);

	Expression* e=scp->getExpression();
	if(e) {
		e->accept(*this);
		context->returnValue = context->currentValue;
	} else {
		foreach(Statement* s, scp->getStatements()) {
			s->accept(*this);
			if(context->returnValue)
				break;
		}
	}

	//"pop" our return value
	Value* v = context->returnValue;
	if(!v)
		v=new Value();

	finishContext();
	context->currentValue=v;
}

void Evaluator::visit(CompoundStatement* stmt)
{
	foreach(Statement* s, stmt->getChildren())
		s->accept(*this);
}

void Evaluator::visit(IfElseStatement* ifelse)
{
	ifelse->getExpression()->accept(*this);
	Value* v=context->currentValue;
	if(v->isTrue()) {
		ifelse->getTrueStatement()->accept(*this);
	} else {
		Statement* falseStmt=ifelse->getFalseStatement();
		if(falseStmt)
			falseStmt->accept(*this);
	}
}

void Evaluator::visit(ForStatement* forstmt)
{
	foreach(Argument* arg, forstmt->getArguments())
		arg->accept(*this);

	//TODO for now just consider the first arg.
	Value* first = context->arguments.at(0);
	startContext(context->currentScope);

	Iterator<Value*>* i = first->createIterator();
	for(i->first(); !i->isDone(); i->next()) {

		Value* v = i->currentItem();
		v->setName(first->getName());
		context->setVariable(v);

		forstmt->getStatement()->accept(*this);

	}
	delete i;
	QList<Node*> childnodes=context->currentNodes;
	finishContext();
	foreach(Node* n,childnodes)
		context->currentNodes.append(n);

	context->arguments.clear();
}

void Evaluator::visit(Parameter* param)
{
	QString name = param->getName();

	Value* v;
	Expression* e = param->getExpression();
	if(e) {
		e->accept(*this);
		v = context->currentValue;
	} else {
		v = new Value();
	}

	v->setName(name);
	context->parameters.append(v);
}

void Evaluator::visit(BinaryExpression* exp)
{
	exp->getLeft()->accept(*this);
	Value* left=context->currentValue;

	exp->getRight()->accept(*this);
	Value* right=context->currentValue;

	Value* result = Value::operation(left,exp->getOp(),right);

	context->currentValue=result;
}

void Evaluator::visit(Argument* arg)
{
	QString name;
	Variable* var = arg->getVariable();
	if(var) {
		var->accept(*this);
		name=context->currentName;
	} else {
		name="";
	}

	arg->getExpression()->accept(*this);
	Value* v = context->currentValue;

	v->setName(name);
	context->arguments.append(v);
}

void Evaluator::visit(AssignStatement* stmt)
{
	stmt->getVariable()->accept(*this);
	QString name = context->currentName;

	Value* lvalue = context->currentValue;

	Value* result;
	switch(stmt->getOperation()) {
	case Expression::Increment: {
		result=Value::operation(lvalue,Expression::Increment);
		break;
	}
	case Expression::Decrement: {
		result=Value::operation(lvalue,Expression::Decrement);
		break;
	}
	default: {
		Expression* expression = stmt->getExpression();
		if(expression) {
			expression->accept(*this);
			result = context->currentValue;
		}
	}
	}

	result->setName(name);
	result->setType(lvalue->getType());
	switch(lvalue->getType()) {
	case Variable::Const:
		if(!context->addVariable(result))
			output << "Warning: Attempt to alter constant variable '" << name << "'\n";
		break;
	case Variable::Param:
		if(!context->addVariable(result))
			output << "Warning: Attempt to alter parametric variable '" << name << "'\n";
		break;
	default:
		context->setVariable(result);
		break;
	}
}

void Evaluator::visit(VectorExpression* exp)
{
	QList<Value*> childvalues;
	foreach(Expression* e, exp->getChildren()) {
		e->accept(*this);
		childvalues.append(context->currentValue);
	}

	Value* v = new VectorValue(childvalues);
	context->currentValue=v;
}

void Evaluator::visit(RangeExpression* exp)
{
	exp->getStart()->accept(*this);
	Value* start = context->currentValue;

	Value* increment = NULL;
	Expression* step = exp->getStep();
	if(step) {
		step->accept(*this);
		increment=context->currentValue;
	}

	exp->getFinish()->accept(*this);
	Value* finish=context->currentValue;

	Value* result = new RangeValue(start,increment,finish);
	context->currentValue=result;
}

void Evaluator::visit(UnaryExpression* exp)
{
	exp->getExpression()->accept(*this);
	Value* left=context->currentValue;

	Value* result = Value::operation(left,exp->getOp());

	context->currentValue=result;
}

void Evaluator::visit(ReturnStatement* stmt)
{
	Expression* e = stmt->getExpression();
	e->accept(*this);
	context->returnValue = context->currentValue;
}

void Evaluator::visit(TernaryExpression* exp)
{
	exp->getCondition()->accept(*this);
	Value* v = context->currentValue;
	if(v->isTrue())
		exp->getTrueExpression()->accept(*this);
	else
		exp->getFalseExpression()->accept(*this);

}

void Evaluator::visit(Invocation* stmt)
{
	QString name = stmt->getName();
	Function* func = context->lookupFunction(name);
	if(func) {
		foreach(Argument* arg, stmt->getArguments())
			arg->accept(*this);

		foreach(Parameter* p, func->getParameters())
			p->accept(*this);

		Scope* scp = func->getScope();
		if(scp)
			scp->accept(*this);

		context->arguments.clear();
		context->parameters.clear();
	}
}

void Evaluator::visit(ModuleImport*)
{
	//TODO
}

void Evaluator::visit(ScriptImport*)
{
	//TODO
}

void Evaluator::visit(Literal* lit)
{
	Value* v= lit->getValue();

	context->currentValue=v;
}

void Evaluator::visit(Variable* var)
{
	QString name = var->getName();
	Variable::Type_e oldType=var->getType();
	Variable::Type_e type=oldType;
	Value* v=context->lookupVariable(name,type);
	if(type!=oldType)
		switch(oldType) {
		case Variable::Const:
			output << "Warning: Attempt to make previously non-constant variable '" << name << "' constant\n";
			break;
		case Variable::Param:
			output << "Warning: Attempt to make previously non-parametric variable '" << name << "' parametric\n";
			break;
		default:
			break;
		}

	context->currentValue=v;
	context->currentName=name;
}

Node* Evaluator::createUnion(QList<Node*> childnodes)
{
	if(childnodes.size()==1) {
		return childnodes.at(0);
	} else {
		UnionNode* u=new UnionNode();
		u->setChildren(childnodes);
		return u;
	}
}

void Evaluator::visit(CodeDoc*)
{
}

void Evaluator::visit(Script* sc)
{
	initBuiltins(sc);

	startContext(sc);
	foreach(Declaration* d, sc->getDeclarations()) {
		d->accept(*this);
	}
	QList<Node*> childnodes=context->currentNodes;

	if(context->returnValue)
		output << "Warning: return statement not valid inside global scope.\n";

	rootNode=createUnion(childnodes);

	saveBuiltins(sc);
}

/**
  To ensure that the builtins do not get deleted when the script
  is deleted we remove them from the script.
*/
void Evaluator::saveBuiltins(Script* sc)
{
	foreach(Declaration* d,Evaluator::builtins)
		sc->removeDeclaration(d);
}

Node* Evaluator::getRootNode() const
{
	return rootNode;
}
