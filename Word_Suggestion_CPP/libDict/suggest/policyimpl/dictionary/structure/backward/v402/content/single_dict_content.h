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
 *   suggest/policyimpl/dictionary/structure/v4/content/single_dict_content.h
 */

#ifndef LATINIME_BACKWARD_V402_SINGLE_DICT_CONTENT_H
#define LATINIME_BACKWARD_V402_SINGLE_DICT_CONTENT_H

#include "../../../../../../../defines.h"
#include "../../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/ver4_dict_constants.h"
#include "../../../../../../../suggest/policyimpl/dictionary/utils/buffer_with_extendable_buffer.h"
#include "../../../../../../../suggest/policyimpl/dictionary/structure/backward/v402/content/dict_content.h"
#include "../../../../../../../suggest/policyimpl/dictionary/utils/dict_file_writing_utils.h"
#include "../../../../../../../suggest/policyimpl/dictionary/utils/mmapped_buffer.h"
#include "../../../../../../../utils/byte_array_view.h"

namespace latinime {
namespace backward {
namespace v402 {

class SingleDictContent : public DictContent {
 public:
    SingleDictContent(const char *const dictPath, const char *const contentFileName,
            const bool isUpdatable)
            : mMmappedBuffer(MmappedBuffer::openBuffer(dictPath, contentFileName, isUpdatable)),
              mExpandableContentBuffer(
                      mMmappedBuffer ? mMmappedBuffer->getReadWriteByteArrayView() :
                              ReadWriteByteArrayView(),
                      BufferWithExtendableBuffer::DEFAULT_MAX_ADDITIONAL_BUFFER_SIZE),
              mIsValid(mMmappedBuffer) {}

    SingleDictContent()
            : mMmappedBuffer(nullptr),
              mExpandableContentBuffer(Ver4DictConstants::MAX_DICTIONARY_SIZE), mIsValid(true) {}

    virtual ~SingleDictContent() {}

    virtual bool isValid() const {
        return mIsValid;
    }

    bool isNearSizeLimit() const {
        return mExpandableContentBuffer.isNearSizeLimit();
    }

 protected:
    BufferWithExtendableBuffer *getWritableBuffer() {
        return &mExpandableContentBuffer;
    }

    const BufferWithExtendableBuffer *getBuffer() const {
        return &mExpandableContentBuffer;
    }

    bool flush(const char *const dictPath, const char *const contentFileNameSuffix) const {
        return DictFileWritingUtils::flushBufferToFileWithSuffix(dictPath,
                contentFileNameSuffix, &mExpandableContentBuffer);
    }

 private:
    DISALLOW_COPY_AND_ASSIGN(SingleDictContent);

    const MmappedBuffer::MmappedBufferPtr mMmappedBuffer;
    BufferWithExtendableBuffer mExpandableContentBuffer;
    const bool mIsValid;
};
} // namespace v402
} // namespace backward
} // namespace latinime
#endif /* LATINIME_BACKWARD_V402_SINGLE_DICT_CONTENT_H */
