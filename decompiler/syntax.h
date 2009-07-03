#ifndef SYNTAX_H
#define SYNTAX_H

#include <cassert>
#include <list>
#include <sstream>
#include <string>

#include "instruction.h"
#include "misc.h"


#include <iostream>
using namespace std;


struct Statement {
	virtual std::string toString(unsigned i) = 0;
	virtual ~Statement() {
	}
};


struct InstructionWrapper : public Statement {

	Instruction *_instruction;

	InstructionWrapper(Instruction *instruction) : _instruction(instruction) {
	};

	std::string toString(unsigned i) {
		return _instruction->toString(i);
	}
};


struct Goto : public Statement {

	address_t _address;

	Goto(address_t address) : _address(address) {
	}

	std::string toString(unsigned i) {
		std::ostringstream ret;
		ret << spaces(i) << "goto " << phex(_address) << std::endl;
		return ret.str();
	}
};


struct WhileLoop : public Statement {

	std::list<InstructionWrapper*> _condition;
	std::list<Statement*> _body;

	std::string toString(unsigned i) {
		std::ostringstream ret;
		ret << std::endl << spaces(i) << "while (" << std::endl;
		foreach (InstructionWrapper* insn, _condition)
			ret << insn->toString(i);
		ret << spaces(i) << ") {" << std::endl;
		foreach (Statement *stmt, _body)
			ret << stmt->toString(i+4);
		ret << spaces(i) << "}" << std::endl << std::endl;
		return ret.str();
	}
};


struct DoWhileLoop : public Statement {

	std::list<InstructionWrapper*> _condition;
	std::list<Statement*> _body;

	std::string toString(unsigned i) {
		std::ostringstream ret;
		ret << std::endl << spaces(i) << "do {" << std::endl;
		foreach (Statement *stmt, _body)
			ret << stmt->toString(i+4);
		ret << spaces(i) << "} while (" << std::endl;
		foreach (InstructionWrapper* insn, _condition)
			ret << insn->toString(i);
		ret << spaces(i) << ")" << std::endl << std::endl;
		return ret.str();
	}
};


struct IfThenElse : public Statement {

	std::list<InstructionWrapper*> _condition;
	std::list<Statement*> _consequence;
	std::list<Statement*> _alternative;

	std::string toString(unsigned i) {
		std::ostringstream ret;
		ret << std::endl << spaces(i) << "if (" << std::endl;
		foreach (InstructionWrapper* insn, _condition)
			ret << insn->toString(i);
		ret << spaces(i) << ") {" << std::endl;
		foreach (Statement *stmt, _consequence)
			ret << stmt->toString(i+4);
		ret << spaces(i) << "}";
		if (!_alternative.empty()) {
			ret << " else {" << std::endl;
			foreach (Statement *stmt, _alternative)
				ret << stmt->toString(i+4);
			ret << spaces(i) << "}";
		}
		ret << std::endl << std::endl;
		return ret.str();
	}
};


void append(Block *block, Block *until, std::list<Statement*> &seq);


IfThenElse *buildIfThenElse(Block *head) {
	IfThenElse *ifte = new IfThenElse;
	foreach (Instruction *insn, head->_instructions)
		ifte->_condition.push_back(new InstructionWrapper(insn));
	append(head->_out.back(), head->_ifFollow, ifte->_consequence);
	append(head->_out.front(), head->_ifFollow, ifte->_alternative);
	return ifte;
}


DoWhileLoop *buildDoWhileLoop(Block *head) {
	DoWhileLoop *loop = new DoWhileLoop;
	foreach (Instruction *insn, head->_loopLatch->_instructions)
		loop->_condition.push_back(new InstructionWrapper(insn));
	append(head, head->_loopLatch, loop->_body);
	return loop;
}


WhileLoop *buildWhileLoop(Block *head) {
	WhileLoop *loop = new WhileLoop;
	foreach (Instruction *insn, head->_instructions)
		loop->_condition.push_back(new InstructionWrapper(insn));
	append(head->nonFollowEdge(), head->_loopFollow, loop->_body);
	return loop;
}

void append(Block *block, Block *until, std::list<Statement*> &seq) {
	if (block == until)
		return;
	if (block->_visited) {
		// TODO they should be printed more before append() only sometimes
		seq.push_back(new Goto(block->_instructions.front()->_addr));
		return;
	}
	block->_visited = true;
	if (block->_loopFollow) {
		if (block->_loopType == PRE_TESTED)
			seq.push_back(buildWhileLoop(block));
		else if (block->_loopType == POST_TESTED)
			seq.push_back(buildDoWhileLoop(block));
		// TODO ENDLESS
		append(block->_loopFollow, until, seq);
		return;
	}
	if (block->_ifFollow) {
		seq.push_back(buildIfThenElse(block));
		append(block->_ifFollow, until, seq);
		return;
	}
	foreach (Instruction *insn, block->_instructions) {
		// TODO jump targets will be broken (pointing to not printed basic blocks) if the jump was rewired
		Jump *jump = dynamic_cast<Jump*>(insn);
		CondJump *condjump = dynamic_cast<CondJump*>(insn);
		if (condjump) {
			IfThenElse *ifte = new IfThenElse;
			ifte->_condition.push_back(new InstructionWrapper(new Instruction("not", 0xffff)));
			ifte->_consequence.push_back(new Goto(jump->target()));
			seq.push_back(ifte);
		} else if (jump)
			seq.push_back(new Goto(jump->target()));
		else
			seq.push_back(new InstructionWrapper(insn));
	}
	if (block->_out.size() == 2) // TODO?
		append(block->_out.back(), until, seq);
	if (block->_out.size() >= 1)
		append(block->_out.front(), until, seq);
}


std::list<Statement*> buildSequence(Block *block, Block *until=0) {
	std::list<Statement*> seq;
	append(block, until, seq);
	return seq;
}


std::list<Statement*> buildAbstractSyntaxTree(ControlFlowGraph &graph) {
	foreach (Block *block, graph._blocks)
		if (dynamic_cast<Jump*>(block->_instructions.back()))
			block->_instructions.pop_back();
	return buildSequence(graph._entry);
}


#endif
