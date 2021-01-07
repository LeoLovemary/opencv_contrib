// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Tencent is pleased to support the open source community by making WeChat QRCode available.
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.

/*
 *  reed_solomondecoder.hpp
 *  zxing
 *
 *  Copyright 2010 ZXing authors All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __ZXING_COMMON_REEDSOLOMON_REEDSOLOMONDECODER_HPP__
#define __ZXING_COMMON_REEDSOLOMON_REEDSOLOMONDECODER_HPP__

#include "zxing/common/array.hpp"
#include "zxing/common/counted.hpp"
#include "zxing/common/reedsolomon/genericgf.hpp"
#include "zxing/common/reedsolomon/genericgfpoly.hpp"
#include "zxing/errorhandler.hpp"

#include <memory>
#include <vector>

namespace zxing {
class GenericGFPoly;
class GenericGF;

class ReedSolomonDecoder {
private:
    Ref<GenericGF> field;

public:
    explicit ReedSolomonDecoder(Ref<GenericGF> fld);
    ~ReedSolomonDecoder();
    void decode(ArrayRef<int> received, int twoS, ErrorHandler &err_handler);
    std::vector<Ref<GenericGFPoly>> runEuclideanAlgorithm(Ref<GenericGFPoly> a,
                                                          Ref<GenericGFPoly> b, int R,
                                                          ErrorHandler &err_handler);

private:
    ArrayRef<int> findErrorLocations(Ref<GenericGFPoly> errorLocator, ErrorHandler &err_handler);
    ArrayRef<int> findErrorMagnitudes(Ref<GenericGFPoly> errorEvaluator,
                                      ArrayRef<int> errorLocations, ErrorHandler &err_handler);
};
}  // namespace zxing

#endif  // __ZXING_COMMON_REEDSOLOMON_REEDSOLOMONDECODER_HPP__