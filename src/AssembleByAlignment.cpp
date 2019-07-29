#include <queue>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include "stream.hpp"
#include "CommonUtils.h"
#include "vg.pb.h"
#include "GfaGraph.h"
#include "Assemble.h"

template <typename T>
class Oriented2dVector
{
public:
	T& operator[](const std::pair<size_t, NodePos>& pos)
	{
		if (pos.second.end) return plus[pos.first][pos.second.id];
		return minus[pos.first][pos.second.id];
	}
	const T& operator[](const std::pair<size_t, NodePos>& pos) const
	{
		if (pos.second.end) return plus[pos.first][pos.second.id];
		return minus[pos.first][pos.second.id];
	}
	void resize(size_t size)
	{
		plus.resize(size);
		minus.resize(size);
	}
	void resizePart(size_t index, size_t size)
	{
		plus[index].resize(size);
		minus[index].resize(size);
	}
private:
	std::vector<std::vector<T>> plus;
	std::vector<std::vector<T>> minus;
};

struct TransitiveClosureMapping
{
	std::map<std::pair<size_t, NodePos>, size_t> mapping;
};

std::pair<NodePos, NodePos> canon(NodePos left, NodePos right)
{
	if (left.id == right.id)
	{
		if (!left.end && !right.end) return std::make_pair(right.Reverse(), left.Reverse());
		return std::make_pair(left, right);
	}
	if (left < right) return std::make_pair(left, right);
	assert(right.Reverse() < left.Reverse());
	return std::make_pair(right.Reverse(), left.Reverse());
}

struct ClosureEdges
{
	std::map<std::pair<NodePos, NodePos>, size_t> coverage;
	std::map<std::pair<NodePos, NodePos>, size_t> overlap;
};

struct DoublestrandedTransitiveClosureMapping
{
	std::map<std::pair<size_t, size_t>, NodePos> mapping;
};

template <typename T>
T find(std::map<T, T>& parent, T key)
{
	if (parent.count(key) == 0)
	{
		parent[key] = key;
		return key;
	}
	if (parent.at(key) == key)
	{
		return key;
	}
	auto result = find(parent, parent.at(key));
	parent[key] = result;
	return result;
}

template <typename T>
void set(std::map<T, T>& parent, T key, T target)
{
	auto found = find(parent, key);
	parent[found] = find(parent, target);
}

template <typename T>
T find(std::unordered_map<T, T>& parent, T key)
{
	if (parent.count(key) == 0)
	{
		parent[key] = key;
		return key;
	}
	if (parent.at(key) == key)
	{
		return key;
	}
	auto result = find(parent, parent.at(key));
	parent[key] = result;
	return result;
}

template <typename T>
void set(std::unordered_map<T, T>& parent, T key, T target)
{
	auto found = find(parent, key);
	parent[found] = find(parent, target);
}

std::pair<size_t, NodePos> find(Oriented2dVector<std::pair<size_t, NodePos>>& parent, std::pair<size_t, NodePos> key)
{
	if (parent[key] == key)
	{
		return key;
	}
	auto result = find(parent, parent[key]);
	parent[key] = result;
	return result;
}

void set(Oriented2dVector<std::pair<size_t, NodePos>>& parent, std::pair<size_t, NodePos> key, std::pair<size_t, NodePos> target)
{
	auto found = find(parent, key);
	parent[found] = find(parent, target);
}

TransitiveClosureMapping getTransitiveClosures(const std::vector<Path>& paths, const std::unordered_set<size_t>& pickedAlns, std::string overlapFile)
{
	TransitiveClosureMapping result;
	Oriented2dVector<std::pair<size_t, NodePos>> parent;
	parent.resize(paths.size());
	for (size_t i = 0; i < paths.size(); i++)
	{
		parent.resizePart(i, paths[i].position.size());
		for (size_t j = 0; j < paths[i].position.size(); j++)
		{
			parent[std::pair<size_t, NodePos> { i, NodePos { j, true } }] = std::pair<size_t, NodePos> { i, NodePos { j, true } };
			parent[std::pair<size_t, NodePos> { i, NodePos { j, false } }] = std::pair<size_t, NodePos> { i, NodePos { j, false } };
		}
	}
	{
		StreamAlignments(overlapFile, [&parent, &pickedAlns](const Alignment& aln){
			bool picked = pickedAlns.count(aln.alignmentID) == 1;
			if (!picked) return;
			for (auto match : aln.alignedPairs)
			{
				std::pair<size_t, NodePos> leftKey { aln.leftPath, NodePos { match.leftIndex, match.leftReverse } };
				std::pair<size_t, NodePos> rightKey { aln.rightPath, NodePos { match.rightIndex, match.rightReverse } };
				set(parent, leftKey, rightKey);
				std::pair<size_t, NodePos> revLeftKey { aln.leftPath, NodePos { match.leftIndex, !match.leftReverse } };
				std::pair<size_t, NodePos> revRightKey { aln.rightPath, NodePos { match.rightIndex, !match.rightReverse } };
				set(parent, revLeftKey, revRightKey);
			}
		});
	}
	std::map<std::pair<size_t, NodePos>, size_t> closureNumber;
	size_t nextClosure = 1;
	for (size_t i = 0; i < paths.size(); i++)
	{
		for (size_t j = 0; j < paths[i].position.size(); j++)
		{
			auto key = std::pair<size_t, NodePos> { i, NodePos { j, true } };
			auto found = find(parent, key);
			if (closureNumber.count(found) == 0)
			{
				closureNumber[found] = nextClosure;
				nextClosure += 1;
			}
			result.mapping[key] = closureNumber.at(found);
			key = std::pair<size_t, NodePos> { i, NodePos { j, false } };
			found = find(parent, key);
			if (closureNumber.count(found) == 0)
			{
				closureNumber[found] = nextClosure;
				nextClosure += 1;
			}
			result.mapping[key] = closureNumber.at(found);
		}
	}
	std::cerr << (nextClosure-1) << " transitive closure sets" << std::endl;
	std::cerr << result.mapping.size() << " transitive closure items" << std::endl;
	return result;
}

GfaGraph getGraph(const DoublestrandedTransitiveClosureMapping& transitiveClosures, const ClosureEdges& edges, const std::vector<Path>& paths, const GfaGraph& graph)
{
	std::unordered_map<size_t, size_t> closureCoverage;
	for (auto pair : transitiveClosures.mapping)
	{
		closureCoverage[pair.second.id] += 1;
	}
	std::unordered_set<size_t> outputtedClosures;
	GfaGraph result;
	result.edgeOverlap = graph.edgeOverlap;
	for (auto pair : transitiveClosures.mapping)
	{
		if (outputtedClosures.count(pair.second.id) == 1) continue;
		NodePos pos = paths[pair.first.first].position[pair.first.second];
		auto seq = graph.nodes.at(pos.id);
		if (!pos.end) seq = CommonUtils::ReverseComplement(seq);
		if (!pair.second.end) seq = CommonUtils::ReverseComplement(seq);
		result.nodes[pair.second.id] = seq;
		result.tags[pair.second.id] = "LN:i:" + std::to_string(seq.size() - graph.edgeOverlap) + "\tRC:i:" + std::to_string((seq.size() - graph.edgeOverlap) * closureCoverage[pair.second.id]) + "\tkm:f:" + std::to_string(closureCoverage[pair.second.id]) + "\toi:Z:" + std::to_string(pos.id) + (pos.end ? "+" : "-");
		outputtedClosures.insert(pair.second.id);
	}
	std::cerr << outputtedClosures.size() << " outputted closures" << std::endl;
	for (auto pair : edges.coverage)
	{
		if (outputtedClosures.count(pair.first.first.id) == 0 || outputtedClosures.count(pair.first.second.id) == 0) continue;
		result.edges[pair.first.first].push_back(pair.first.second);
		result.edgeTags[std::make_pair(pair.first.first, pair.first.second)] = "RC:i:" + std::to_string(pair.second);
	}
	result.varyingOverlaps.insert(edges.overlap.begin(), edges.overlap.end());
	std::cerr << edges.coverage.size() << " outputted edges" << std::endl;
	return result;
}

DoublestrandedTransitiveClosureMapping mergeDoublestrandClosures(const std::vector<Path>& paths, const TransitiveClosureMapping& original)
{
	DoublestrandedTransitiveClosureMapping result;
	std::unordered_map<size_t, NodePos> mapping;
	int nextId = 1;
	for (size_t i = 0; i < paths.size(); i++)
	{
		for (size_t j = 0; j < paths[i].position.size(); j++)
		{
			std::pair<size_t, NodePos> fwKey { i, NodePos { j, true } };
			std::pair<size_t, NodePos> bwKey { i, NodePos { j, false } };
			assert(original.mapping.count(fwKey) == 1);
			assert(original.mapping.count(bwKey) == 1);
			size_t fwSet = original.mapping.at(fwKey);
			size_t bwSet = original.mapping.at(bwKey);
			assert(mapping.count(fwSet) == mapping.count(bwSet));
			if (mapping.count(fwSet) == 0)
			{
				if (fwSet == bwSet)
				{
					mapping[fwSet] = NodePos { nextId, true };
					assert(false);
				}
				else
				{
					mapping[fwSet] = NodePos { nextId, true };
					mapping[bwSet] = NodePos { nextId, false };
				}
				nextId += 1;
			}
			assert(mapping.count(fwSet) == 1);
			result.mapping[std::pair<size_t, size_t> { i, j }] = mapping.at(fwSet);
		}
	}
	std::cerr << (nextId-1) << " doublestranded transitive closure sets" << std::endl;
	return result;
}

// std::vector<Alignment> doubleAlignments(const std::vector<Alignment>& alns)
// {
// 	std::vector<Alignment> result = alns;
// 	result.reserve(alns.size() * 2);
// 	for (auto aln : alns)
// 	{
// 		result.emplace_back();
// 		result.back().alignmentLength = aln.alignmentLength;
// 		result.back().alignmentIdentity = aln.alignmentIdentity;
// 		result.back().leftPath = aln.leftPath;
// 		result.back().rightPath = aln.rightPath;
// 		result.back().alignedPairs = aln.alignedPairs;
// 		for (size_t i = 0; i < result.back().alignedPairs.size(); i++)
// 		{
// 			result.back().alignedPairs[i].leftReverse = !result.back().alignedPairs[i].leftReverse;
// 			result.back().alignedPairs[i].rightReverse = !result.back().alignedPairs[i].rightReverse;
// 		}
// 	}
// 	std::cerr << result.size() << " alignments after doubling" << std::endl;
// 	return result;
// }

std::vector<Alignment> removeContained(const std::vector<Path>& paths, const std::vector<Alignment>& original)
{
	std::vector<std::vector<size_t>> continuousEnd;
	continuousEnd.resize(paths.size());
	for (size_t i = 0; i < paths.size(); i++)
	{
		continuousEnd[i].resize(paths[i].position.size(), 0);
	}
	for (auto aln : original)
	{
		assert(aln.leftStart <= aln.leftEnd);
		assert(aln.rightStart <= aln.rightEnd);
		for (size_t i = aln.leftStart; i <= aln.leftEnd; i++)
		{
			continuousEnd[aln.leftPath][i] = std::max(continuousEnd[aln.leftPath][i], aln.leftEnd);
		}
		for (size_t i = aln.rightStart; i <= aln.rightEnd; i++)
		{
			continuousEnd[aln.rightPath][i] = std::max(continuousEnd[aln.rightPath][i], aln.rightEnd);
		}
	}
	std::vector<Alignment> result;
	for (auto aln : original)
	{
		if (continuousEnd[aln.leftPath][aln.leftStart] > aln.leftEnd) continue;
		if (aln.leftStart > 0 && continuousEnd[aln.leftPath][aln.leftStart-1] >= aln.leftEnd) continue;
		if (continuousEnd[aln.rightPath][aln.rightStart] > aln.rightEnd) continue;
		if (aln.rightStart > 0 && continuousEnd[aln.rightPath][aln.rightStart-1] >= aln.rightEnd) continue;
		result.push_back(aln);
	}
	std::cerr << result.size() << " alignments after removing contained" << std::endl;
	return result;
}

// std::vector<Alignment> pickLowestErrorPerRead(const std::vector<Path>& paths, const std::vector<Alignment>& alns, size_t maxNum)
// {
// 	std::vector<std::vector<Alignment>> alnsPerRead;
// 	alnsPerRead.resize(paths.size());
// 	for (auto aln : alns)
// 	{
// 		alnsPerRead[aln.leftPath].push_back(aln);
// 		alnsPerRead[aln.rightPath].push_back(aln);
// 	}
// 	std::vector<Alignment> result;
// 	for (size_t i = 0; i < alnsPerRead.size(); i++)
// 	{
// 		if (alnsPerRead[i].size() > maxNum)
// 		{
// 			std::sort(alnsPerRead[i].begin(), alnsPerRead[i].end(), [](const Alignment& left, const Alignment& right){ return left.alignmentIdentity < right.alignmentIdentity; });
// 			for (size_t j = alnsPerRead[i].size() - maxNum; j < alnsPerRead[i].size(); j++)
// 			{
// 				result.push_back(alnsPerRead[i][j]);
// 			}
// 		}
// 		else
// 		{
// 			result.insert(result.end(), alnsPerRead[i].begin(), alnsPerRead[i].end());
// 		}
// 	}
// 	std::cerr << result.size() << " alignments after picking lowest erro" << std::endl;
// 	return result;
// }

void assignToGroupRec(size_t group, size_t i, size_t j, std::vector<std::vector<size_t>>& belongsToGroup, const std::vector<std::vector<std::vector<std::pair<size_t, size_t>>>>& edges, std::vector<std::vector<std::pair<size_t, size_t>>>& groups)
{
	assert(i < belongsToGroup.size());
	assert(i < edges.size());
	assert(j < belongsToGroup[i].size());
	assert(j < edges[i].size());
	if (group == belongsToGroup[i][j]) return;
	assert(belongsToGroup[i][j] == std::numeric_limits<size_t>::max());
	belongsToGroup[i][j] = group;
	groups[group].emplace_back(i, j);
	for (auto edge : edges[i][j])
	{
		assignToGroupRec(group, edge.first, edge.second, belongsToGroup, edges, groups);
	}
}

void addAffectedNodesRec(size_t i, const std::vector<std::vector<std::pair<size_t, size_t>>>& edges, const std::vector<bool>& forbidden, std::unordered_set<size_t>& affectedNodes)
{
	if (affectedNodes.count(i) == 1) return;
	affectedNodes.insert(i);
	for (auto edge : edges[i])
	{
		if (forbidden[edge.second]) continue;
		addAffectedNodesRec(edge.first, edges, forbidden, affectedNodes);
	}
}

size_t find(size_t n, std::vector<size_t>& parent)
{
	if (parent[n] == n) return n;
	auto result = parent[n];
	parent[n] = result;
	return result;
}

void merge(size_t left, size_t right, std::vector<size_t>& parent, std::vector<size_t>& componentSize)
{
	auto leftp = find(left, parent);
	auto rightp = find(right, parent);
	if (componentSize[leftp] > componentSize[rightp])
	{
		std::swap(left, right);
		std::swap(leftp, rightp);
	}
	parent[leftp] = rightp;
	componentSize[rightp] += componentSize[leftp];
}

bool maybeAllow(size_t allowThis, std::vector<size_t>& parent, std::vector<size_t>& componentSize, std::vector<bool>& allowed, const std::vector<Alignment>& alns, const std::vector<std::vector<size_t>>& nodeNum, int coverageDifferenceCutoff)
{
	bool hasCoverageDifference = false;
	for (size_t i = 0; i < alns[allowThis].alignedPairs.size(); i++)
	{
		size_t thisNode = nodeNum[alns[allowThis].leftPath][alns[allowThis].alignedPairs[i].leftIndex];
		size_t thisOtherNode = nodeNum[alns[allowThis].rightPath][alns[allowThis].alignedPairs[i].rightIndex];
		if (find(thisNode, parent) == find(thisOtherNode, parent)) continue;
		int countHere = componentSize[find(thisNode, parent)] + componentSize[find(thisOtherNode, parent)];
		if (alns[allowThis].alignedPairs[i].leftIndex > 0)
		{
			if (i == 0 || alns[allowThis].alignedPairs[i-1].leftIndex != alns[allowThis].alignedPairs[i].leftIndex-1)
			{
				size_t compareNode = nodeNum[alns[allowThis].leftPath][alns[allowThis].alignedPairs[i].leftIndex-1];
				int otherCount = componentSize[find(compareNode, parent)];
				if (countHere >= coverageDifferenceCutoff && otherCount >= coverageDifferenceCutoff && countHere - otherCount > coverageDifferenceCutoff) hasCoverageDifference = true;
			}
		}
		if (alns[allowThis].alignedPairs[i].rightIndex > 0)
		{
			if (i == 0 || alns[allowThis].alignedPairs[i-1].rightIndex != alns[allowThis].alignedPairs[i].rightIndex-1)
			{
				size_t compareNode = nodeNum[alns[allowThis].rightPath][alns[allowThis].alignedPairs[i].rightIndex-1];
				int otherCount = componentSize[find(compareNode, parent)];
				if (countHere >= coverageDifferenceCutoff && otherCount >= coverageDifferenceCutoff && countHere - otherCount > coverageDifferenceCutoff) hasCoverageDifference = true;
			}
		}
		if (alns[allowThis].alignedPairs[i].leftIndex < nodeNum[alns[allowThis].leftPath].size() - 1)
		{
			if (i == alns[allowThis].alignedPairs.size()-1 || alns[allowThis].alignedPairs[i+1].leftIndex != alns[allowThis].alignedPairs[i].leftIndex+1)
			{
				size_t compareNode = nodeNum[alns[allowThis].leftPath][alns[allowThis].alignedPairs[i].leftIndex+1];
				int otherCount = componentSize[find(compareNode, parent)];
				if (countHere >= coverageDifferenceCutoff && otherCount >= coverageDifferenceCutoff && countHere - otherCount > coverageDifferenceCutoff) hasCoverageDifference = true;
			}
		}
		if (alns[allowThis].alignedPairs[i].rightIndex < nodeNum[alns[allowThis].rightPath].size() - 1)
		{
			if (i == alns[allowThis].alignedPairs.size()-1 || alns[allowThis].alignedPairs[i+1].rightIndex != alns[allowThis].alignedPairs[i].rightIndex+1)
			{
				size_t compareNode = nodeNum[alns[allowThis].rightPath][alns[allowThis].alignedPairs[i].rightIndex+1];
				int otherCount = componentSize[find(compareNode, parent)];
				if (countHere >= coverageDifferenceCutoff && otherCount >= coverageDifferenceCutoff && countHere - otherCount > coverageDifferenceCutoff) hasCoverageDifference = true;
			}
		}
	}
	if (hasCoverageDifference) return false;
	assert(!allowed[allowThis]);
	allowed[allowThis] = true;
	for (auto pair : alns[allowThis].alignedPairs)
	{
		size_t thisNode = nodeNum[alns[allowThis].leftPath][pair.leftIndex];
		size_t thisOtherNode = nodeNum[alns[allowThis].rightPath][pair.rightIndex];
		merge(thisNode, thisOtherNode, parent, componentSize);
	}
	return true;
}

// std::unordered_set<size_t> zipAddAlignments(const std::vector<Path>& paths, const std::unordered_set<size_t>& initialPick, std::string alnFile, int coverageDifferenceCutoff)
// {
// 	std::vector<Alignment> alns;
// 	std::vector<bool> picked;
// 	StreamAlignments(alnFile, [&alns, &initialPick](const Alignment& aln){
// 		if (initialPick.count(aln.alignmentID) == 0) return;
// 		alns.emplace_back(aln);
// 	});
// 	std::cerr << alns.size() << " overlaps" << std::endl;
// 	size_t nodeCount = 0;
// 	picked.resize(alns.size(), false);
// 	std::vector<std::vector<size_t>> nodeNum;
// 	nodeNum.resize(paths.size());
// 	for (size_t i = 0; i < paths.size(); i++)
// 	{
// 		nodeNum[i].resize(paths[i].position.size());
// 		for (size_t j = 0; j < paths[i].position.size(); j++)
// 		{
// 			nodeNum[i][j] = nodeCount;
// 			nodeCount++;
// 		}
// 	}
// 	std::cerr << nodeCount << " nodes" << std::endl;
// 	std::vector<size_t> parent;
// 	std::vector<size_t> componentSize;
// 	componentSize.resize(nodeCount, 1);
// 	parent.resize(nodeCount, 0);
// 	for (size_t i = 0; i < nodeCount; i++)
// 	{
// 		parent[i] = i;
// 	}
// 	std::vector<size_t> worstAlignments;
// 	worstAlignments.reserve(alns.size());
// 	for (size_t i = 0; i < alns.size(); i++)
// 	{
// 		worstAlignments.push_back(i);
// 	}
// 	std::sort(worstAlignments.begin(), worstAlignments.end(), [&alns](size_t left, size_t right) { return alns[left].alignmentLength * alns[left].alignmentIdentity < alns[right].alignmentLength * alns[right].alignmentIdentity; });
// 	std::cerr << "allow from best to worst" << std::endl;
// 	size_t allowedCount = 0;
// 	for (size_t i = worstAlignments.size()-1; i < worstAlignments.size(); i--)
// 	{
// 		size_t aln = worstAlignments[i];
// 		assert(!picked[aln]);
// 		bool allowed = maybeAllow(aln, parent, componentSize, picked, alns, nodeNum, coverageDifferenceCutoff);
// 		if (allowed) std::cerr << "!"; else std::cerr << ".";
// 		if (allowed) allowedCount += 1;
// 	}
// 	std::cerr << allowedCount << " allowed overlaps" << std::endl;
// 	std::unordered_set<size_t> result;
// 	for (size_t i = 0; i < alns.size(); i++)
// 	{
// 		if (!picked[i]) continue;
// 		size_t key = alns[i].alignmentID;
// 		assert(result.count(key) == 0);
// 		result.insert(key);
// 	}
// 	std::cerr << result.size() << " allowed overlaps" << std::endl;
// 	assert(result.size() == allowedCount);
// 	return result;
// }

struct OverlapQueueItem
{
	size_t node;
	size_t group;
	double score;
	OverlapQueueItem(size_t node, size_t group, double score) :
	node(node), group(group), score(score)
	{}
	OverlapQueueItem() = default;
	bool operator<(const OverlapQueueItem& other) const
	{
		return score < other.score;
	}
};

void addForbiddenOverlaps(const double matchScore, const double mismatchScore, std::unordered_set<size_t>& forbiddenOverlaps, const std::unordered_set<size_t>& nodes, const std::vector<std::unordered_set<size_t>>& overlapsPerRead, const std::vector<Alignment>& alns, const double groupCutoff)
{
	std::unordered_set<size_t> checkedEdges;
	size_t oldForbiddenSize = forbiddenOverlaps.size();
	while (true)
	{
		double biggestNegative = 0;
		size_t biggestNegativeIndex = alns.size();
		std::vector<std::unordered_map<size_t, double>> scoreToGroup;
		std::unordered_map<size_t, size_t> currentGroup;
		scoreToGroup.resize(3);
		for (auto node : nodes)
		{
			for (auto overlap : overlapsPerRead[node])
			{
				if (checkedEdges.count(overlap) == 1) continue;
				double scoreHere = (double)alns[overlap].matches * matchScore + (double)alns[overlap].mismatches * mismatchScore;
				if (scoreHere < biggestNegative)
				{
					biggestNegative = scoreHere;
					biggestNegativeIndex = overlap;
				}
			}
		}
		if (biggestNegative >= 0) break;
		assert(biggestNegative < 0);
		// if (biggestNegative > -groupCutoff) return;
		checkedEdges.insert(biggestNegativeIndex);
		assert(biggestNegativeIndex < alns.size());
		assert(alns[biggestNegativeIndex].leftPath != alns[biggestNegativeIndex].rightPath);
		std::priority_queue<OverlapQueueItem> queue;
		currentGroup[alns[biggestNegativeIndex].leftPath] = 1;
		currentGroup[alns[biggestNegativeIndex].rightPath] = 2;
		assert(nodes.count(alns[biggestNegativeIndex].leftPath) == 1);
		assert(nodes.count(alns[biggestNegativeIndex].rightPath) == 1);
		for (auto edge : overlapsPerRead[alns[biggestNegativeIndex].leftPath])
		{
			checkedEdges.insert(edge);
			size_t other = alns[edge].leftPath;
			if (other == alns[biggestNegativeIndex].leftPath) other = alns[edge].rightPath;
			scoreToGroup[0][other] -= matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
			scoreToGroup[1][other] += matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
			scoreToGroup[2][other] -= matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
			queue.emplace(other, 0, scoreToGroup[0][other]);
			queue.emplace(other, 1, scoreToGroup[1][other]);
			queue.emplace(other, 2, scoreToGroup[2][other]);
		}
		for (auto edge : overlapsPerRead[alns[biggestNegativeIndex].rightPath])
		{
			checkedEdges.insert(edge);
			size_t other = alns[edge].leftPath;
			if (other == alns[biggestNegativeIndex].rightPath) other = alns[edge].rightPath;
			scoreToGroup[0][other] -= matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
			scoreToGroup[1][other] -= matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
			scoreToGroup[2][other] += matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
			queue.emplace(other, 0, scoreToGroup[0][other]);
			queue.emplace(other, 1, scoreToGroup[1][other]);
			queue.emplace(other, 2, scoreToGroup[2][other]);
		}
		while (queue.size() > 0)
		{
			auto top = queue.top();
			queue.pop();
			size_t node = top.node;
			size_t group = top.group;
			double score = top.score;
			if (currentGroup.count(node) == 1) continue;
			if (scoreToGroup[group][node] < score - 0.01) continue;
			if (score < groupCutoff) continue;
			if (group == 0)
			{
				group = scoreToGroup.size();
				scoreToGroup.emplace_back(scoreToGroup[0]);
			}
			currentGroup[node] = group;
			for (auto edge : overlapsPerRead[node])
			{
				checkedEdges.insert(edge);
				size_t other = alns[edge].leftPath;
				if (other == node) other = alns[edge].rightPath;
				// if (nodes.count(node) == 0 && nodes.count(other) == 0) continue;
				if (currentGroup.count(other) == 1) continue;
				//twice because loop decrements it once
				scoreToGroup[group][other] += matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
				scoreToGroup[group][other] += matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
				for (size_t i = 0; i < scoreToGroup.size(); i++)
				{
					scoreToGroup[i][other] -= matchScore * (double)alns[edge].matches + mismatchScore * (double)alns[edge].mismatches;
					queue.emplace(other, i, scoreToGroup[i][other]);
				}
			}
		}
		for (auto pair : currentGroup)
		{
			size_t node = pair.first;
			size_t group = pair.second;
			for (auto edge : overlapsPerRead[node])
			{
				size_t other = alns[edge].leftPath;
				if (other == node) other = alns[edge].rightPath;
				if (currentGroup.count(other) == 1 && currentGroup.at(other) != group)
				{
					forbiddenOverlaps.insert(alns[edge].alignmentID);
				}
			}
		}
	}
	std::cerr << nodes.size() << " " << (forbiddenOverlaps.size() - oldForbiddenSize) << std::endl;
}

bool isAllowedToMerge(std::vector<size_t>& parent, const std::vector<size_t>& left, const std::vector<size_t>& right, const std::unordered_map<size_t, std::unordered_set<size_t>>& forbiddenMerges, const Alignment& aln, const std::vector<std::vector<size_t>>& pathToNode)
{
	for (auto pair : aln.alignedPairs)
	{
		auto leftp = find(pathToNode[aln.leftPath][pair.leftIndex], parent);
		auto rightp = find(pathToNode[aln.rightPath][pair.rightIndex], parent);
		if (forbiddenMerges.count(leftp) == 0) continue;
		if (forbiddenMerges.at(leftp).count(rightp) == 1) return false;
	}
	return true;
}

void merge(std::vector<size_t>& parent, std::vector<size_t>& left, std::vector<size_t>& right, std::unordered_map<size_t, std::unordered_set<size_t>>& forbiddenMerges, const Alignment& aln, const std::vector<std::vector<size_t>>& pathToNode)
{
	for (auto pair : aln.alignedPairs)
	{
		auto leftNode = pathToNode[aln.leftPath][pair.leftIndex];
		auto rightNode = pathToNode[aln.rightPath][pair.rightIndex];
		auto leftp = find(leftNode, parent);
		auto rightp = find(rightNode, parent);
		if (leftp == rightp) continue;
		parent[rightp] = leftp;
		size_t leftRight = right[leftp];
		size_t rightRight = right[rightp];
		left[rightRight] = leftp;
		right[leftp] = rightRight;
		left[leftRight] = rightp;
		right[rightp] = leftRight;
		if (forbiddenMerges.count(rightp) == 1)
		{
			size_t leftForbidden = rightp;
			for (auto rightForbidden : forbiddenMerges.at(rightp))
			{
				forbiddenMerges[find(leftForbidden, parent)].insert(find(rightForbidden, parent));
				forbiddenMerges[find(rightForbidden, parent)].insert(find(leftForbidden, parent));
			}
		}
	}
}

std::unordered_set<size_t> pickNonForbiddenMergingAlns(const std::vector<Path>& paths, const std::string& alnFile, double zeroIdentity, double groupCutoff)
{
	double matchScore = (1.0 - zeroIdentity);
	double mismatchScore = - zeroIdentity;
	std::vector<Alignment> alns;
	std::vector<std::vector<size_t>> pathToNode;
	std::vector<std::unordered_map<size_t, std::vector<size_t>>> nodesInPath;
	size_t nextNodeId = 0;
	for (size_t i = 0; i < paths.size(); i++)
	{
		pathToNode.emplace_back();
		nodesInPath.emplace_back();
		for (size_t j = 0; j < paths[i].position.size(); j++)
		{
			pathToNode[i].emplace_back(nextNodeId);
			nodesInPath[i][paths[i].position[j].id].emplace_back(nextNodeId);
			nextNodeId += 1;
		}
	}
	std::unordered_map<size_t, std::unordered_set<size_t>> forbiddenMerges;
	size_t numForbiddenMerges = 0;
	StreamAlignments(alnFile, [&alns, &forbiddenMerges, &nodesInPath, &paths, &pathToNode, &numForbiddenMerges, matchScore, mismatchScore](Alignment& aln){
		assert(aln.alignmentID == alns.size());
		assert(aln.leftPath < pathToNode.size());
		assert(aln.rightPath < pathToNode.size());
		if (aln.matches * matchScore + aln.mismatches * mismatchScore < 0)
		{
			for (size_t i = 0; i < paths[aln.leftPath].position.size(); i++)
			{
				auto node = paths[aln.leftPath].position[i].id;
				size_t left = pathToNode[aln.leftPath][i];
				if (nodesInPath[aln.rightPath].count(node) == 0) continue;
				for (auto right : nodesInPath[aln.rightPath][node])
				{
					forbiddenMerges[left].insert(right);
					forbiddenMerges[right].insert(left);
					numForbiddenMerges += 1;
				}
			}
		}
		alns.emplace_back(std::move(aln));
	});
	std::cerr << alns.size() << " raw overlaps" << std::endl;
	std::sort(alns.begin(), alns.end(), [matchScore, mismatchScore](const Alignment& left, const Alignment& right) { return left.matches * matchScore + left.mismatches * mismatchScore < right.matches * matchScore + right.mismatches * mismatchScore; });
	std::cerr << numForbiddenMerges << " forbidden merges" << std::endl;
	std::vector<size_t> parent;
	std::vector<size_t> left;
	std::vector<size_t> right;
	parent.reserve(nextNodeId);
	left.reserve(nextNodeId);
	right.reserve(nextNodeId);
	for (size_t i = 0; i < nextNodeId; i++)
	{
		parent.push_back(i);
		left.push_back(i);
		right.push_back(i);
	}
	std::unordered_set<size_t> forbiddenOverlaps;
	for (size_t i = alns.size()-1; i < alns.size(); i--)
	{
		const Alignment& aln = alns[i];
		if (matchScore * aln.matches + mismatchScore * aln.mismatches < groupCutoff)
		{
			forbiddenOverlaps.insert(aln.alignmentID);
			continue;
		}
		if (!isAllowedToMerge(parent, left, right, forbiddenMerges, aln, pathToNode))
		{
			forbiddenOverlaps.insert(aln.alignmentID);
			continue;
		}
		merge(parent, left, right, forbiddenMerges, aln, pathToNode);
	}
	std::cerr << forbiddenOverlaps.size() << " forbidden overlaps" << std::endl;
	std::unordered_set<size_t> result;
	for (size_t i = 0; i < alns.size(); i++)
	{
		if (forbiddenOverlaps.count(i) == 1) continue;
		result.insert(i);
	}
	std::cerr << result.size() << " allowed overlaps" << std::endl;
	return result;
}

// std::unordered_set<size_t> pickLongestPerRead(const std::vector<Path>& paths, std::string alnFile, size_t maxNum)
// {
// 	std::vector<Alignment> alns;
// 	StreamAlignments(alnFile, [&alns](const Alignment& aln){
// 		alns.emplace_back(aln);
// 		decltype(aln.alignedPairs) tmp;
// 		std::swap(tmp, alns.back().alignedPairs);
// 	});
// 	std::cerr << alns.size() << " raw overlaps" << std::endl;
// 	std::vector<std::vector<size_t>> leftAlnsPerRead;
// 	std::vector<std::vector<size_t>> rightAlnsPerRead;
// 	std::vector<int> picked;
// 	picked.resize(alns.size(), 0);
// 	leftAlnsPerRead.resize(paths.size());
// 	rightAlnsPerRead.resize(paths.size());
// 	for (size_t i = 0; i < alns.size(); i++)
// 	{
// 		assert(alns[i].leftPath < paths.size());
// 		assert(alns[i].rightPath < paths.size());
// 		assert(alns[i].leftEnd < paths[alns[i].leftPath].position.size());
// 		assert(alns[i].rightEnd < paths[alns[i].rightPath].position.size());
// 		if (alns[i].leftStart == 0) leftAlnsPerRead[alns[i].leftPath].push_back(i);
// 		if (alns[i].leftEnd == paths[alns[i].leftPath].position.size()-1) rightAlnsPerRead[alns[i].leftPath].push_back(i);
// 		if (alns[i].rightStart == 0) leftAlnsPerRead[alns[i].rightPath].push_back(i);
// 		if (alns[i].rightEnd == paths[alns[i].rightPath].position.size()-1) rightAlnsPerRead[alns[i].rightPath].push_back(i);
// 	}
// 	for (size_t i = 0; i < leftAlnsPerRead.size(); i++)
// 	{
// 		std::sort(leftAlnsPerRead[i].begin(), leftAlnsPerRead[i].end(), AlignmentMatchComparerLT { alns });
// 		std::sort(rightAlnsPerRead[i].begin(), rightAlnsPerRead[i].end(), AlignmentMatchComparerLT { alns });
// 		std::set<size_t> pickedHere;
// 		for (size_t j = leftAlnsPerRead[i].size() > maxNum ? (leftAlnsPerRead[i].size() - maxNum) : 0; j < leftAlnsPerRead[i].size(); j++)
// 		{
// 			pickedHere.insert(leftAlnsPerRead[i][j]);
// 		}
// 		for (size_t j = rightAlnsPerRead[i].size() > maxNum ? (rightAlnsPerRead[i].size() - maxNum) : 0; j < rightAlnsPerRead[i].size(); j++)
// 		{
// 			pickedHere.insert(rightAlnsPerRead[i][j]);
// 		}
// 		for (auto index : pickedHere)
// 		{
// 			picked[index] += 1;
// 		}
// 		std::sort(leftAlnsPerRead[i].begin(), leftAlnsPerRead[i].end(), AlignmentQualityComparerLT { alns });
// 		std::sort(rightAlnsPerRead[i].begin(), rightAlnsPerRead[i].end(), AlignmentQualityComparerLT { alns });
// 		pickedHere.clear();
// 		for (size_t j = leftAlnsPerRead[i].size() > maxNum ? (leftAlnsPerRead[i].size() - maxNum) : 0; j < leftAlnsPerRead[i].size(); j++)
// 		{
// 			pickedHere.insert(leftAlnsPerRead[i][j]);
// 		}
// 		for (size_t j = rightAlnsPerRead[i].size() > maxNum ? (rightAlnsPerRead[i].size() - maxNum) : 0; j < rightAlnsPerRead[i].size(); j++)
// 		{
// 			pickedHere.insert(rightAlnsPerRead[i][j]);
// 		}
// 		for (auto index : pickedHere)
// 		{
// 			picked[index] += 1;
// 		}
// 	}
// 	std::unordered_set<size_t> result;
// 	for (size_t i = 0; i < alns.size(); i++)
// 	{
// 		assert(picked[i] >= 0);
// 		assert(picked[i] <= 4);
// 		if (picked[i] == 4)
// 		{
// 			assert(result.count(alns[i].alignmentID) == 0);
// 			result.emplace(alns[i].alignmentID);
// 		}
// 	}
// 	std::cerr << result.size() << " alignments after picking longest" << std::endl;
// 	return result;
// }

DoublestrandedTransitiveClosureMapping removeOutsideCoverageClosures(const DoublestrandedTransitiveClosureMapping& closures, int minCoverage, int maxCoverage)
{
	std::unordered_map<size_t, size_t> coverage;
	for (auto pair : closures.mapping)
	{
		coverage[pair.second.id] += 1;
	}
	DoublestrandedTransitiveClosureMapping result;
	std::unordered_set<size_t> numbers;
	for (auto pair : closures.mapping)
	{
		if (coverage[pair.second.id] >= minCoverage && coverage[pair.second.id] <= maxCoverage)
		{
			result.mapping[pair.first] = pair.second;
			numbers.insert(pair.second.id);
		}
	}
	std::cerr << numbers.size() << " closures after removing low coverage" << std::endl;
	std::cerr << result.mapping.size() << " closure items after removing low coverage" << std::endl;
	return result;
}

DoublestrandedTransitiveClosureMapping insertMiddles(const DoublestrandedTransitiveClosureMapping& original, const std::vector<Path>& paths)
{
	DoublestrandedTransitiveClosureMapping result;
	result = original;
	int nextNum = 0;
	for (auto pair : original.mapping)
	{
		nextNum = std::max(nextNum, pair.second.id);
	}
	nextNum =+ 1;
	for (size_t i = 0; i < paths.size(); i++)
	{
		size_t firstExisting = paths[i].position.size();
		size_t lastExisting = paths[i].position.size();
		for (size_t j = 0; j < paths[i].position.size(); j++)
		{
			std::pair<size_t, size_t> key { i, j };
			if (original.mapping.count(key) == 1)
			{
				if (firstExisting == paths[i].position.size()) firstExisting = j;
				lastExisting = j;
			}
		}
		for (size_t j = firstExisting; j < lastExisting; j++)
		{
			std::pair<size_t, size_t> key { i, j };
			if (original.mapping.count(key) == 1) continue;
			result.mapping[key] = NodePos { nextNum, true };
			nextNum += 1;
		}
	}
	std::cerr << nextNum << " transitive closure sets after inserting middles" << std::endl;
	std::cerr << result.mapping.size() << " transitive closure items after inserting middles" << std::endl;
	return result;
}

std::vector<Alignment> removeHighCoverageAlignments(const std::vector<Path>& paths, const std::vector<Alignment>& alns, size_t maxCoverage)
{
	std::vector<std::vector<size_t>> alnsPerRead;
	std::vector<bool> validAln;
	validAln.resize(alns.size(), true);
	alnsPerRead.resize(paths.size());
	for (size_t i = 0; i < alns.size(); i++)
	{
		alnsPerRead[alns[i].leftPath].push_back(i);
		alnsPerRead[alns[i].rightPath].push_back(i);
	}
	for (size_t i = 0; i < paths.size(); i++)
	{
		std::vector<size_t> startCount;
		std::vector<size_t> endCount;
		startCount.resize(paths[i].position.size(), 0);
		endCount.resize(paths[i].position.size(), 0);
		for (auto alnIndex : alnsPerRead[i])
		{
			auto aln = alns[alnIndex];
			if (aln.leftPath == i)
			{
				startCount[aln.leftStart] += 1;
				endCount[aln.leftEnd] += 1;
			}
			else
			{
				startCount[aln.rightStart] += 1;
				endCount[aln.rightEnd] += 1;
			}
		}
		std::vector<size_t> coverage;
		coverage.resize(paths[i].position.size(), 0);
		coverage[0] = startCount[0];
		for (size_t j = 1; j < coverage.size(); j++)
		{
			coverage[j] = coverage[j-1] + startCount[j] - endCount[j-1];
		}
		for (auto alnIndex : alnsPerRead[i])
		{
			auto aln = alns[alnIndex];
			bool valid = false;
			size_t start, end;
			if (aln.leftPath == i)
			{
				start = aln.leftStart;
				end = aln.leftEnd;
			}
			else
			{
				start = aln.rightStart;
				end = aln.rightEnd;
			}
			for (size_t j = start; j <= end; j++)
			{
				if (coverage[j] <= maxCoverage)
				{
					valid = true;
					break;
				}
			}
			if (!valid)
			{
				validAln[alnIndex] = false;
			}
		}
	}
	std::vector<Alignment> result;
	for (size_t i = 0; i < validAln.size(); i++)
	{
		if (validAln[i]) result.push_back(alns[i]);
	}
	std::cerr << result.size() << " after removing high coverage alignments" << std::endl;
	return result;
}

std::vector<Alignment> removeNonDovetails(const std::vector<Path>& paths, const std::vector<Alignment>& alns)
{
	std::vector<Alignment> result;
	for (auto aln : alns)
	{
		if (aln.leftStart == 0) continue;
		if (aln.leftEnd != paths[aln.leftPath].position.size()-1) continue;
		if (aln.rightReverse)
		{
			if (aln.rightStart == 0) continue;
			if (aln.rightEnd != paths[aln.rightPath].position.size()-1) continue;
		}
		else
		{
			if (aln.rightStart != 0) continue;
			if (aln.rightEnd == paths[aln.rightPath].position.size()-1) continue;
		}
		result.push_back(aln);
	}
	std::cerr << result.size() << " alignments after removing non-dovetails" << std::endl;
	return result;
}

ClosureEdges getClosureEdges(const DoublestrandedTransitiveClosureMapping& closures, const std::vector<Path>& paths)
{
	ClosureEdges result;
	for (size_t i = 0; i < paths.size(); i++)
	{
		for (size_t j = 1; j < paths[i].position.size(); j++)
		{
			if (closures.mapping.count(std::make_pair(i, j-1)) == 0) continue;
			if (closures.mapping.count(std::make_pair(i, j)) == 0) continue;
			NodePos oldPos = closures.mapping.at(std::make_pair(i, j-1));
			NodePos newPos = closures.mapping.at(std::make_pair(i, j));
			result.coverage[canon(oldPos, newPos)] += 1;
		}
	}
	std::cerr << result.coverage.size() << " edges" << std::endl;
	return result;
}

ClosureEdges removeChimericEdges(const DoublestrandedTransitiveClosureMapping& closures, const ClosureEdges& edges, size_t maxRemovableCoverage, double fraction)
{
	std::unordered_map<NodePos, size_t> maxOutEdgeCoverage;
	for (auto edge : edges.coverage)
	{
		maxOutEdgeCoverage[edge.first.first] = std::max(maxOutEdgeCoverage[edge.first.first], edge.second);
		maxOutEdgeCoverage[edge.first.second.Reverse()] = std::max(maxOutEdgeCoverage[edge.first.second.Reverse()], edge.second);
	}
	ClosureEdges result;
	for (auto edge : edges.coverage)
	{
		if (edge.second <= maxRemovableCoverage)
		{
			if ((double)edge.second < (double)maxOutEdgeCoverage[edge.first.first] * fraction) continue;
			if ((double)edge.second < (double)maxOutEdgeCoverage[edge.first.second.Reverse()] * fraction) continue;
		}
		result.coverage[edge.first] = edge.second;
	}
	std::cerr << result.coverage.size() << " edges after chimeric removal" << std::endl;
	return result;
}

std::pair<DoublestrandedTransitiveClosureMapping, ClosureEdges> bridgeTips(const DoublestrandedTransitiveClosureMapping& closures, const ClosureEdges& edges, const std::vector<Path>& paths, size_t minCoverage)
{
	std::unordered_set<NodePos> isNotTip;
	for (auto pair : edges.coverage)
	{
		isNotTip.insert(pair.first.first);
		isNotTip.insert(pair.first.second.Reverse());
	}
	std::unordered_map<std::pair<NodePos, NodePos>, std::vector<std::tuple<size_t, size_t, size_t>>> pathsSupportingEdge;
	for (size_t i = 0; i < paths.size(); i++)
	{
		std::vector<size_t> gapStarts;
		for (size_t j = 1; j < paths[i].position.size(); j++)
		{
			auto currentKey = std::make_pair(i, j);
			auto previousKey = std::make_pair(i, j-1);
			if (closures.mapping.count(previousKey) == 1 && isNotTip.count(closures.mapping.at(previousKey)) == 0)
			{
				gapStarts.push_back(j-1);
			}
			if (closures.mapping.count(currentKey) == 1 && isNotTip.count(closures.mapping.at(currentKey).Reverse()) == 0)
			{
				auto endPos = closures.mapping.at(currentKey);
				for (auto start : gapStarts)
				{
					auto startPos = closures.mapping.at(std::make_pair(i, start));
					pathsSupportingEdge[canon(startPos, endPos)].emplace_back(i, start, j);
				}
			}
		}
	}
	DoublestrandedTransitiveClosureMapping resultClosures = closures;
	ClosureEdges resultEdges = edges;
	for (auto pair : pathsSupportingEdge)
	{
		std::set<size_t> readsSupportingPath;
		for (auto t : pair.second)
		{
			readsSupportingPath.insert(std::get<0>(t));
		}
		if (readsSupportingPath.size() >= minCoverage)
		{
			resultEdges.coverage[pair.first] = readsSupportingPath.size();
		}
	}
	std::cerr << resultEdges.coverage.size() << " edges after bridging tips" << std::endl;
	return std::make_pair(resultClosures, resultEdges);
}

size_t getLongestOverlap(const std::string& left, const std::string& right, size_t maxOverlap)
{
	assert(left.size() >= maxOverlap);
	assert(right.size() >= maxOverlap);
	for (size_t i = maxOverlap; i > 0; i--)
	{
		if (left.substr(left.size()-maxOverlap) == right.substr(0, maxOverlap)) return i;
	}
	return 0;
}

ClosureEdges determineClosureOverlaps(const std::vector<Path>& paths, const DoublestrandedTransitiveClosureMapping& closures, const ClosureEdges& edges, const GfaGraph& graph)
{
	ClosureEdges result;
	std::unordered_map<size_t, NodePos> closureRepresentsNode;
	for (auto pair : closures.mapping)
	{
		assert(pair.first.first < paths.size());
		assert(pair.first.second < paths[pair.first.first].position.size());
		NodePos pos = paths[pair.first.first].position[pair.first.second];
		assert(graph.nodes.count(pos.id) == 1);
		if (!pair.second.end) pos = pos.Reverse();
		assert(closureRepresentsNode.count(pair.second.id) == 0 || closureRepresentsNode.at(pair.second.id) == pos);
		closureRepresentsNode[pair.second.id] = pos;
	}
	for (auto pair : edges.coverage)
	{
		NodePos fromClosure = pair.first.first;
		NodePos toClosure = pair.first.second;
		if (closureRepresentsNode.count(fromClosure.id) == 0) continue;
		if (closureRepresentsNode.count(toClosure.id) == 0) continue;
		result.coverage[pair.first] = pair.second;
		auto key = std::make_pair(fromClosure, toClosure);
		if (graph.varyingOverlaps.count(key) == 1)
		{
			result.overlap[key] = graph.varyingOverlaps.at(key);
			continue;
		}
		assert(closureRepresentsNode.count(fromClosure.id) == 1);
		assert(closureRepresentsNode.count(toClosure.id) == 1);
		NodePos fromNode = closureRepresentsNode[fromClosure.id];
		if (!fromClosure.end) fromNode = fromNode.Reverse();
		NodePos toNode = closureRepresentsNode[toClosure.id];
		if (!toClosure.end) toNode = toNode.Reverse();
		bool hasEdge = false;
		if (graph.edges.count(fromNode) == 1)
		{
			for (auto target : graph.edges.at(fromNode))
			{
				if (target == toNode) hasEdge = true;
			}
		}
		if (hasEdge)
		{
			result.overlap[key] = graph.edgeOverlap;
			continue;
		}
		assert(graph.nodes.count(fromNode.id) == 1);
		assert(graph.nodes.count(toNode.id) == 1);
		std::string before = graph.nodes.at(fromNode.id);
		if (!fromNode.end) before = CommonUtils::ReverseComplement(before);
		std::string after = graph.nodes.at(toNode.id);
		if (!toNode.end) after = CommonUtils::ReverseComplement(after);
		result.overlap[key] = getLongestOverlap(before, after, graph.edgeOverlap);
	}
	return result;
}

void outputRemappedReads(std::string filename, const std::vector<Path>& paths, const DoublestrandedTransitiveClosureMapping& closures, const ClosureEdges& edges)
{
	std::vector<vg::Alignment> alns;
	for (size_t i = 0; i < paths.size(); i++)
	{
		std::vector<NodePos> translated;
		for (size_t j = 0; j < paths[j].position.size(); j++)
		{
			auto key = std::make_pair(i, j);
			if (closures.mapping.count(key) == 0) continue;
			translated.push_back(closures.mapping.at(key));
		}
		if (translated.size() == 0) continue;
		std::vector<std::vector<NodePos>> validSubpaths;
		validSubpaths.emplace_back();
		validSubpaths.back().push_back(translated[0]);
		for (size_t j = 1; j < translated.size(); j++)
		{
			bool hasEdge = edges.coverage.count(canon(translated[j-1], translated[j])) == 1;
			if (!hasEdge)
			{
				assert(validSubpaths.back().size() != 0);
				validSubpaths.emplace_back();
				validSubpaths.back().push_back(translated[j]);
				continue;
			}
			validSubpaths.back().push_back(translated[j]);
		}
		size_t currentNum = 0;
		for (size_t j = 0; j < validSubpaths.size(); j++)
		{
			if (validSubpaths[j].size() == 0) continue;
			alns.emplace_back();
			alns.back().set_name(paths[i].name + "_" + std::to_string(currentNum));
			currentNum++;
			for (size_t k = 0; k < validSubpaths[j].size(); k++)
			{
				auto mapping = alns.back().mutable_path()->add_mapping();
				mapping->mutable_position()->set_node_id(validSubpaths[j][k].id);
				mapping->mutable_position()->set_is_reverse(!validSubpaths[j][k].end);
			}
		}
	}

	std::ofstream alignmentOut { filename, std::ios::out | std::ios::binary };
	stream::write_buffered(alignmentOut, alns, 0);
}

int main(int argc, char** argv)
{
	std::string inputGraph { argv[1] };
	std::string inputAlns { argv[2] };
	std::string inputOverlaps { argv[3] };
	std::string outputGraph { argv[4] };
	std::string outputPaths { argv[5] };
	// int initialMaxPick = std::stoi(argv[6]);
	double zeroIdentity = std::stod(argv[6]);
	double groupCutoff = std::stod(argv[7]);
	// int coverageDifferenceCutoff = std::stoi(argv[7]);

	std::cerr << "load graph" << std::endl;
	auto graph = GfaGraph::LoadFromFile(inputGraph);
	graph.confirmDoublesidedEdges();
	std::vector<Path> paths;
	{
		auto nodeSizes = getNodeSizes(graph);
		std::cerr << "load paths" << std::endl;
		paths = loadAlignmentsAsPaths(inputAlns, 1000, nodeSizes);
		std::cerr << paths.size() << " paths after filtering by length" << std::endl;
	}
	std::cerr << "pick nonforbidden merging alignments" << std::endl;
	auto pickedAlns = pickNonForbiddenMergingAlns(paths, inputOverlaps, zeroIdentity, groupCutoff);
	// std::cerr << "pick longest alignments" << std::endl;
	// auto longestAlns = pickLongestPerRead(paths, inputOverlaps, initialMaxPick);
	// std::cerr << "pick-add alignments" << std::endl;
	// auto pickedAlns = zipAddAlignments(paths, longestAlns, inputOverlaps, coverageDifferenceCutoff);
	std::cerr << "get transitive closure" << std::endl;
	auto transitiveClosures = getTransitiveClosures(paths, pickedAlns, inputOverlaps);
	std::cerr << "deallocate picked" << std::endl;
	{
		decltype(pickedAlns) tmp;
		std::swap(pickedAlns, tmp);
		// decltype(longestAlns) tmp2;
		// std::swap(longestAlns, tmp2);
	}
	std::cerr << "merge double strands" << std::endl;
	auto doubleStrandedClosures = mergeDoublestrandClosures(paths, transitiveClosures);
	std::cerr << "deallocate one-stranded closures" << std::endl;
	{
		decltype(transitiveClosures) tmp;
		std::swap(transitiveClosures, tmp);
	}
	std::cerr << "remove wrong coverage closures" << std::endl;
	doubleStrandedClosures = removeOutsideCoverageClosures(doubleStrandedClosures, 3, 10000);
	std::cerr << "get closure edges" << std::endl;
	auto closureEdges = getClosureEdges(doubleStrandedClosures, paths);
	std::cerr << "bridge tips" << std::endl;
	std::tie(doubleStrandedClosures, closureEdges) = bridgeTips(doubleStrandedClosures, closureEdges, paths, 3);
	// std::cerr << "insert middles" << std::endl;
	// doubleStrandedClosures = insertMiddles(doubleStrandedClosures, paths);
	std::cerr << "remove chimeric edges" << std::endl;
	closureEdges = removeChimericEdges(doubleStrandedClosures, closureEdges, 5, 0.2);
	closureEdges = removeChimericEdges(doubleStrandedClosures, closureEdges, 10, 0.1);
	std::cerr << "determine closure overlaps" << std::endl;
	closureEdges = determineClosureOverlaps(paths, doubleStrandedClosures, closureEdges, graph);
	std::cerr << "graphify" << std::endl;
	auto result = getGraph(doubleStrandedClosures, closureEdges, paths, graph);
	std::cerr << "output graph" << std::endl;
	result.SaveToFile(outputGraph);
	std::cerr << "output translated paths" << std::endl;
	outputRemappedReads(outputPaths, paths, doubleStrandedClosures, closureEdges);
}