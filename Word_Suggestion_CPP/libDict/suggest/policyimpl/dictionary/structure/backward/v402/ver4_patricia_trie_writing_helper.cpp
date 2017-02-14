/*
 * Copyright (C) 2013, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * !!!!! DO NOT EDIT THIS FILE !!!!!
 *
 * This file was generated from
 *   suggest/policyimpl/dictionary/structure/v4/ver4_patricia_trie_writing_helper.cpp
 */

#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_patricia_trie_writing_helper.h"

#include <cstring>
#include <queue>

#include "../../../../../../suggest/policyimpl/dictionary/header/header_policy.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/bigram/ver4_bigram_list_policy.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/shortcut/ver4_shortcut_list_policy.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_dict_buffers.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_dict_constants.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_patricia_trie_node_reader.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_patricia_trie_node_writer.h"
#include "../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_pt_node_array_reader.h"
#include "../../../../../../suggest/policyimpl/dictionary/utils/buffer_with_extendable_buffer.h"
#include "../../../../../../suggest/policyimpl/dictionary/utils/file_utils.h"
#include "../../../../../../suggest/policyimpl/dictionary/utils/forgetting_curve_utils.h"

namespace latinime {
namespace backward {
namespace v402 {

bool Ver4PatriciaTrieWritingHelper::writeToDictFile(const char *const dictDirPath,
        const int unigramCount, const int bigramCount) const {
    const HeaderPolicy *const headerPolicy = mBuffers->getHeaderPolicy();
    BufferWithExtendableBuffer headerBuffer(
            BufferWithExtendableBuffer::DEFAULT_MAX_ADDITIONAL_BUFFER_SIZE);
    const int extendedRegionSize = headerPolicy->getExtendedRegionSize()
            + mBuffers->getTrieBuffer()->getUsedAdditionalBufferSize();
    if (!headerPolicy->fillInAndWriteHeaderToBuffer(false /* updatesLastDecayedTime */,
            unigramCount, bigramCount, extendedRegionSize, &headerBuffer)) {
        AKLOGE("Cannot write header structure to buffer. "
                "updatesLastDecayedTime: %d, unigramCount: %d, bigramCount: %d, "
                "extendedRegionSize: %d", false, unigramCount, bigramCount,
                extendedRegionSize);
        return false;
    }
    return mBuffers->flushHeaderAndDictBuffers(dictDirPath, &headerBuffer);
}

bool Ver4PatriciaTrieWritingHelper::writeToDictFileWithGC(const int rootPtNodeArrayPos,
        const char *const dictDirPath) {
    const HeaderPolicy *const headerPolicy = mBuffers->getHeaderPolicy();
    Ver4DictBuffers::Ver4DictBuffersPtr dictBuffers(
            Ver4DictBuffers::createVer4DictBuffers(headerPolicy,
                    Ver4DictConstants::MAX_DICTIONARY_SIZE));
    int unigramCount = 0;
    int bigramCount = 0;
    if (!runGC(rootPtNodeArrayPos, headerPolicy, dictBuffers.get(), &unigramCount, &bigramCount)) {
        return false;
    }
    BufferWithExtendableBuffer headerBuffer(
            BufferWithExtendableBuffer::DEFAULT_MAX_ADDITIONAL_BUFFER_SIZE);
    if (!headerPolicy->fillInAndWriteHeaderToBuffer(true /* updatesLastDecayedTime */,
            unigramCount, bigramCount, 0 /* extendedRegionSize */, &headerBuffer)) {
        return false;
    }
    return dictBuffers->flushHeaderAndDictBuffers(dictDirPath, &headerBuffer);
}

bool Ver4PatriciaTrieWritingHelper::runGC(const int rootPtNodeArrayPos,
        const HeaderPolicy *const headerPolicy, Ver4DictBuffers *const buffersToWrite,
        int *const outUnigramCount, int *const outBigramCount) {
    Ver4PatriciaTrieNodeReader ptNodeReader(mBuffers->getTrieBuffer(),
            mBuffers->getProbabilityDictContent(), headerPolicy);
    Ver4PtNodeArrayReader ptNodeArrayReader(mBuffers->getTrieBuffer());
    Ver4BigramListPolicy bigramPolicy(mBuffers->getMutableBigramDictContent(),
            mBuffers->getTerminalPositionLookupTable(), headerPolicy);
    Ver4ShortcutListPolicy shortcutPolicy(mBuffers->getMutableShortcutDictContent(),
            mBuffers->getTerminalPositionLookupTable());
    Ver4PatriciaTrieNodeWriter ptNodeWriter(mBuffers->getWritableTrieBuffer(),
            mBuffers, headerPolicy, &ptNodeReader, &ptNodeArrayReader, &bigramPolicy,
            &shortcutPolicy);

    DynamicPtReadingHelper readingHelper(&ptNodeReader, &ptNodeArrayReader);
    readingHelper.initWithPtNodeArrayPos(rootPtNodeArrayPos);
    DynamicPtGcEventListeners
            ::TraversePolicyToUpdateUnigramProbabilityAndMarkUselessPtNodesAsDeleted
                    traversePolicyToUpdateUnigramProbabilityAndMarkUselessPtNodesAsDeleted(
                            &ptNodeWriter);
    if (!readingHelper.traverseAllPtNodesInPostorderDepthFirstManner(
            &traversePolicyToUpdateUnigramProbabilityAndMarkUselessPtNodesAsDeleted)) {
        return false;
    }
    const int unigramCount = traversePolicyToUpdateUnigramProbabilityAndMarkUselessPtNodesAsDeleted
            .getValidUnigramCount();
    const int maxUnigramCount = headerPolicy->getMaxUnigramCount();
    if (headerPolicy->isDecayingDict() && unigramCount > maxUnigramCount) {
        if (!truncateUnigrams(&ptNodeReader, &ptNodeWriter, maxUnigramCount)) {
            AKLOGE("Cannot remove unigrams. current: %d, max: %d", unigramCount,
                    maxUnigramCount);
            return false;
        }
    }

    readingHelper.initWithPtNodeArrayPos(rootPtNodeArrayPos);
    DynamicPtGcEventListeners::TraversePolicyToUpdateBigramProbability
            traversePolicyToUpdateBigramProbability(&ptNodeWriter);
    if (!readingHelper.traverseAllPtNodesInPostorderDepthFirstManner(
            &traversePolicyToUpdateBigramProbability)) {
        return false;
    }
    const int bigramCount = traversePolicyToUpdateBigramProbability.getValidBigramEntryCount();
    const int maxBigramCount = headerPolicy->getMaxBigramCount();
    if (headerPolicy->isDecayingDict() && bigramCount > maxBigramCount) {
        if (!truncateBigrams(maxBigramCount)) {
            AKLOGE("Cannot remove bigrams. current: %d, max: %d", bigramCount, maxBigramCount);
            return false;
        }
    }

    // Mapping from positions in mBuffer to positions in bufferToWrite.
    PtNodeWriter::DictPositionRelocationMap dictPositionRelocationMap;
    readingHelper.initWithPtNodeArrayPos(rootPtNodeArrayPos);
    Ver4PatriciaTrieNodeWriter ptNodeWriterForNewBuffers(buffersToWrite->getWritableTrieBuffer(),
            buffersToWrite, headerPolicy, &ptNodeReader, &ptNodeArrayReader, &bigramPolicy,
            &shortcutPolicy);
    DynamicPtGcEventListeners::TraversePolicyToPlaceAndWriteValidPtNodesToBuffer
            traversePolicyToPlaceAndWriteValidPtNodesToBuffer(&ptNodeWriterForNewBuffers,
                    buffersToWrite->getWritableTrieBuffer(), &dictPositionRelocationMap);
    if (!readingHelper.traverseAllPtNodesInPtNodeArrayLevelPreorderDepthFirstManner(
            &traversePolicyToPlaceAndWriteValidPtNodesToBuffer)) {
        return false;
    }

    // Create policy instances for the GCed dictionary.
    Ver4PatriciaTrieNodeReader newPtNodeReader(buffersToWrite->getTrieBuffer(),
            buffersToWrite->getProbabilityDictContent(), headerPolicy);
    Ver4PtNodeArrayReader newPtNodeArrayreader(buffersToWrite->getTrieBuffer());
    Ver4BigramListPolicy newBigramPolicy(buffersToWrite->getMutableBigramDictContent(),
            buffersToWrite->getTerminalPositionLookupTable(), headerPolicy);
    Ver4ShortcutListPolicy newShortcutPolicy(buffersToWrite->getMutableShortcutDictContent(),
            buffersToWrite->getTerminalPositionLookupTable());
    Ver4PatriciaTrieNodeWriter newPtNodeWriter(buffersToWrite->getWritableTrieBuffer(),
            buffersToWrite, headerPolicy, &newPtNodeReader, &newPtNodeArrayreader, &newBigramPolicy,
            &newShortcutPolicy);
    // Re-assign terminal IDs for valid terminal PtNodes.
    TerminalPositionLookupTable::TerminalIdMap terminalIdMap;
    if(!buffersToWrite->getMutableTerminalPositionLookupTable()->runGCTerminalIds(
            &terminalIdMap)) {
        return false;
    }
    // Run GC for probability dict content.
    if (!buffersToWrite->getMutableProbabilityDictContent()->runGC(&terminalIdMap,
            mBuffers->getProbabilityDictContent())) {
        return false;
    }
    // Run GC for bigram dict content.
    if(!buffersToWrite->getMutableBigramDictContent()->runGC(&terminalIdMap,
            mBuffers->getBigramDictContent(), outBigramCount)) {
        return false;
    }
    // Run GC for shortcut dict content.
    if(!buffersToWrite->getMutableShortcutDictContent()->runGC(&terminalIdMap,
            mBuffers->getShortcutDictContent())) {
        return false;
    }
    DynamicPtReadingHelper newDictReadingHelper(&newPtNodeReader, &newPtNodeArrayreader);
    newDictReadingHelper.initWithPtNodeArrayPos(rootPtNodeArrayPos);
    DynamicPtGcEventListeners::TraversePolicyToUpdateAllPositionFields
            traversePolicyToUpdateAllPositionFields(&newPtNodeWriter, &dictPositionRelocationMap);
    if (!newDictReadingHelper.traverseAllPtNodesInPtNodeArrayLevelPreorderDepthFirstManner(
            &traversePolicyToUpdateAllPositionFields)) {
        return false;
    }
    newDictReadingHelper.initWithPtNodeArrayPos(rootPtNodeArrayPos);
    TraversePolicyToUpdateAllPtNodeFlagsAndTerminalIds
            traversePolicyToUpdateAllPtNodeFlagsAndTerminalIds(&newPtNodeWriter, &terminalIdMap);
    if (!newDictReadingHelper.traverseAllPtNodesInPostorderDepthFirstManner(
            &traversePolicyToUpdateAllPtNodeFlagsAndTerminalIds)) {
        return false;
    }
    *outUnigramCount = traversePolicyToUpdateAllPositionFields.getUnigramCount();
    return true;
}

bool Ver4PatriciaTrieWritingHelper::truncateUnigrams(
        const Ver4PatriciaTrieNodeReader *const ptNodeReader,
        Ver4PatriciaTrieNodeWriter *const ptNodeWriter, const int maxUnigramCount) {
    const TerminalPositionLookupTable *const terminalPosLookupTable =
            mBuffers->getTerminalPositionLookupTable();
    const int nextTerminalId = terminalPosLookupTable->getNextTerminalId();
    std::priority_queue<DictProbability, std::vector<DictProbability>, DictProbabilityComparator>
            priorityQueue;
    for (int i = 0; i < nextTerminalId; ++i) {
        const int terminalPos = terminalPosLookupTable->getTerminalPtNodePosition(i);
        if (terminalPos == NOT_A_DICT_POS) {
            continue;
        }
        const ProbabilityEntry probabilityEntry =
                mBuffers->getProbabilityDictContent()->getProbabilityEntry(i);
        const int probability = probabilityEntry.hasHistoricalInfo() ?
                ForgettingCurveUtils::decodeProbability(
                        probabilityEntry.getHistoricalInfo(), mBuffers->getHeaderPolicy()) :
                probabilityEntry.getProbability();
        priorityQueue.push(DictProbability(terminalPos, probability,
                probabilityEntry.getHistoricalInfo()->getTimeStamp()));
    }

    // Delete unigrams.
    while (static_cast<int>(priorityQueue.size()) > maxUnigramCount) {
        const int ptNodePos = priorityQueue.top().getDictPos();
        priorityQueue.pop();
        const PtNodeParams ptNodeParams =
                ptNodeReader->fetchPtNodeParamsInBufferFromPtNodePos(ptNodePos);
        if (ptNodeParams.representsNonWordInfo()) {
            continue;
        }
        if (!ptNodeWriter->markPtNodeAsWillBecomeNonTerminal(&ptNodeParams)) {
            AKLOGE("Cannot mark PtNode as willBecomeNonterminal. PtNode pos: %d", ptNodePos);
            return false;
        }
    }
    return true;
}

bool Ver4PatriciaTrieWritingHelper::truncateBigrams(const int maxBigramCount) {
    const TerminalPositionLookupTable *const terminalPosLookupTable =
            mBuffers->getTerminalPositionLookupTable();
    const int nextTerminalId = terminalPosLookupTable->getNextTerminalId();
    std::priority_queue<DictProbability, std::vector<DictProbability>, DictProbabilityComparator>
            priorityQueue;
    BigramDictContent *const bigramDictContent = mBuffers->getMutableBigramDictContent();
    for (int i = 0; i < nextTerminalId; ++i) {
        const int bigramListPos = bigramDictContent->getBigramListHeadPos(i);
        if (bigramListPos == NOT_A_DICT_POS) {
            continue;
        }
        bool hasNext = true;
        int readingPos = bigramListPos;
        while (hasNext) {
            const int entryPos = readingPos;
            const BigramEntry bigramEntry =
                    bigramDictContent->getBigramEntryAndAdvancePosition(&readingPos);
            hasNext = bigramEntry.hasNext();
            if (!bigramEntry.isValid()) {
                continue;
            }
            const int probability = bigramEntry.hasHistoricalInfo() ?
                    ForgettingCurveUtils::decodeProbability(
                            bigramEntry.getHistoricalInfo(), mBuffers->getHeaderPolicy()) :
                    bigramEntry.getProbability();
            priorityQueue.push(DictProbability(entryPos, probability,
                    bigramEntry.getHistoricalInfo()->getTimeStamp()));
        }
    }

    // Delete bigrams.
    while (static_cast<int>(priorityQueue.size()) > maxBigramCount) {
        const int entryPos = priorityQueue.top().getDictPos();
        const BigramEntry bigramEntry = bigramDictContent->getBigramEntry(entryPos);
        const BigramEntry invalidatedBigramEntry = bigramEntry.getInvalidatedEntry();
        if (!bigramDictContent->writeBigramEntry(&invalidatedBigramEntry, entryPos)) {
            AKLOGE("Cannot write bigram entry to remove. pos: %d", entryPos);
            return false;
        }
        priorityQueue.pop();
    }
    return true;
}

bool Ver4PatriciaTrieWritingHelper::TraversePolicyToUpdateAllPtNodeFlagsAndTerminalIds
        ::onVisitingPtNode(const PtNodeParams *const ptNodeParams) {
    if (!ptNodeParams->isTerminal()) {
        return true;
    }
    TerminalPositionLookupTable::TerminalIdMap::const_iterator it =
            mTerminalIdMap->find(ptNodeParams->getTerminalId());
    if (it == mTerminalIdMap->end()) {
        AKLOGE("terminal Id %d is not in the terminal position map. map size: %zd",
                ptNodeParams->getTerminalId(), mTerminalIdMap->size());
        return false;
    }
    if (!mPtNodeWriter->updateTerminalId(ptNodeParams, it->second)) {
        AKLOGE("Cannot update terminal id. %d -> %d", it->first, it->second);
    }
    return mPtNodeWriter->updatePtNodeHasBigramsAndShortcutTargetsFlags(ptNodeParams);
}

} // namespace v402
} // namespace backward
} // namespace latinime
