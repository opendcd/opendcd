
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: allauzen@google.com (Cyril Allauzen)
// Copyright 2014 Paul R. Dixon
// Author: Paul R. Dixon

#include <fst/fst.h>
#include <fst/matcher-fst.h>

using fst::FstRegisterer;
using fst::LogArc;
using fst::StdArc;


namespace fst {
typedef MatcherFst<ConstFst<StdArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<StdArc> >,
                                         olabel_lookahead_flags,
                                         CacheLogAccumulator<StdArc> >,
                   olabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<StdArc> > StdOLabelCacheLookAheadFst;

typedef MatcherFst<ConstFst<LogArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<LogArc> >,
                                         olabel_lookahead_flags,
                                         CacheLogAccumulator<LogArc> >,
                   olabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<LogArc> > LogOLabelCacheLookAheadFst;
}


// Register OLabelLookAhead Fsts with common arc types
static FstRegisterer<fst::StdOLabelCacheLookAheadFst>
OLabelCacheLookAheadFst_StdArc_registerer;

static FstRegisterer<fst::LogOLabelCacheLookAheadFst>
OLabelCacheLookAheadFst_LogArc_registerer;
