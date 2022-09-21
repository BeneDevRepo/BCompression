#pragma once


#include <cstdint>

#include <vector>
#include <queue>

#include <iostream>


class Huffman { // used as namespace for static functions
private:
	struct Node {
		size_t symbol;
		size_t frequency;
		Node *child0;
		Node *child1;

		struct SortFunc {
			bool operator()(Node *a, Node *b) const {
				return a->frequency > b->frequency;
			}
		};

		Node(const size_t symbol, const size_t frequency):
			symbol(symbol), frequency(frequency),
			child0(nullptr), child1(nullptr) { }
	};

public:
	inline static std::vector<size_t> calcCodeLengths(const std::vector<size_t>& frequencies) {
		std::vector<size_t> codeLengths(frequencies.size()); // contains the code-length of each symbol

		Node *tree = createTree(frequencies);
		extractCodeLengths(tree, codeLengths);
		freeTree(tree);

		return codeLengths;
	}

	inline static std::vector<size_t> calcCodeLengths(const std::vector<size_t>& frequencies, const size_t MAX_CODE_LENGTH) {
		std::vector<size_t> codeLengths = calcCodeLengths(frequencies);
		restrictCodeLengths(codeLengths, MAX_CODE_LENGTH);

		return codeLengths;
	}


private:
	inline static Node* createTree(const std::vector<size_t>& frequencies) {
		std::priority_queue<Node*, std::vector<Node*>, Node::SortFunc> tree;

		for(size_t symbol = 0; symbol < frequencies.size(); symbol++)
			if(frequencies[symbol] > 0)
				tree.push(new Node(symbol, frequencies[symbol]));
		

		// ensure that at least 2 symbols exist (to ensure proper function of huffman algorithm):
		if(tree.size() == 0)
			tree.push(new Node(0, 1));
		if(tree.size() == 1)
			tree.push(new Node((tree.top()->symbol == 0) ? 1 : 0, 1));


		// iterate until everything is merged into one tree
		for(; tree.size() > 1; ) {
			Node *low0 = tree.top();
			tree.pop();

			Node *low1 = tree.top();
			tree.pop();

			Node *parent = new Node(-1, low0->frequency + low1->frequency);
			parent->child0 = low0;
			parent->child1 = low1;

			tree.push(parent);
		}

		// if(tree.size() == 0)
		// 	return new Node(0, 0);

		return tree.top();
	}

	inline static void restrictCodeLengths(std::vector<size_t>& codeLengths, const size_t MAX_CODE_LENGTH) {
		for(;;) {
			bool tooLong1 = false; // true if at least one found
			size_t tooLongSymbol1;
			for(size_t i = 0; i < codeLengths.size(); i++) {
				if(codeLengths[i] > 15) {
					if(tooLong1 == false || codeLengths[i] > codeLengths[tooLongSymbol1])
						tooLongSymbol1 = i;
					tooLong1 = true;
				}
			}

			if(!tooLong1)
				break; // Job Done. No code is too long

			bool tooLong2 = false; // true if at least one found
			size_t tooLongSymbol2;
			for(size_t i = 0; i < codeLengths.size(); i++) {
				if(i == tooLongSymbol1) continue;
				if(codeLengths[i] == codeLengths[tooLongSymbol1]) {
					tooLong2 = true;
					tooLongSymbol2 = i;
					break;
				}
			}

			if(!tooLong2)
				throw std::runtime_error("Huffman length-reduction failed");

			bool longestAcceptable = false; // true if at least one found
			size_t longestAcceptableSymbol;
			for(size_t i = 0; i < codeLengths.size(); i++) {
				if(codeLengths[i] < 15) {
					if(longestAcceptable == false || codeLengths[i] > codeLengths[longestAcceptableSymbol])
						longestAcceptableSymbol = i;
					longestAcceptable = true;

					if(codeLengths[i] == 15 - 1) break; // already found optimal solution.
				}
			}

			codeLengths[longestAcceptableSymbol]++;
			codeLengths[tooLongSymbol1] = codeLengths[longestAcceptableSymbol];
			codeLengths[tooLongSymbol2]--;
		}
	}

	inline static void freeTree(Node *tree) {
		if(tree == nullptr) return;

		freeTree(tree->child0);
		freeTree(tree->child1);

		delete tree;
	}

	inline static void extractCodeLengths(const Node *const node, std::vector<size_t>& codeLengths, const size_t currentDepth = 0) {
		if(node == nullptr)
			return;

		if(node->symbol != -1)
			codeLengths[node->symbol] = currentDepth;

		extractCodeLengths(node->child0, codeLengths, currentDepth + 1);
		extractCodeLengths(node->child1, codeLengths, currentDepth + 1);
	}
};