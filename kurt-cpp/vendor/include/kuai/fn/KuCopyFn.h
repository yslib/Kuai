#pragma once

#include <iterator>
#include <type_traits>

#include <kuai/core/KuContext.h>
#include <kuai/core/KuCore.h>
#include <kuai/ktl/KuView.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/ktl/iterator/KuTransformIterator.h>
#include <kuai/ktl/iterator/KuViewIterator.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/copy.h"

namespace kuai {

template <typename Span, typename... Trs>
struct KuIsCompactView {
    static constexpr bool value =
        std::is_same_v<typename KuView<Span, Trs...>::view_category, CompactionView_t>;
};

template <typename Args>
struct KuIsView : std::false_type {};

template <typename Span, typename... Trs>
struct KuIsView<KuView<Span, Trs...>> : std::true_type {};

template <typename Vendor>
struct KuCopyFn {
public:
    KuCopyFn(KuContext &context) : m_device(context) {
    }

    explicit KuCopyFn(const KuVendorContext<Vendor> &device) : m_device(device) {
    }

    template <typename Span1,
              typename Span2,
              typename... Trs1,
              typename... Trs2,
              typename = std::enable_if_t<!KuIsCompactView<Span1, Trs1...>::value
                                          && !KuIsCompactView<Span2, Trs2...>::value>>
    auto operator()(KuView<Span1, Trs1...> &dst, const KuView<Span2, Trs2...> &src) {
        using V1 = KuView<Span1, Trs1...>;
        using V2 = KuView<Span2, Trs2...>;
        static_assert(V1::rank() == V2::rank(), "Rank mismatch between views");
        auto exts1 = dst.extents();
        auto exts2 = src.extents();
        for (std::size_t i = 0; i < exts1.rank(); ++i) {
            if (exts1.extent(i) != exts2.extent(i)) {
                throw std::runtime_error("shape mismatch between views");
            }
        }

        using T1 = typename std::iterator_traits<typename V1::data_handle_type>::value_type;
        using T2 = typename std::iterator_traits<typename V2::data_handle_type>::value_type;
        auto size = exts1.prod();
        auto begin = makeKuViewInputIterator(src, ku_size_t(0));
        auto end = makeKuViewInputIterator(src, ku_size_t(size));
        auto castBegin = makeTransformIterator(begin, ku_cast<T1>());
        auto castEnd = makeTransformIterator(end, ku_cast<T1>());
        auto output = makeKuViewOutputIterator(dst, ku_size_t(0));
        // return exts1;
        algo::copy(m_device, output, castBegin, castEnd);
        return exts1;
    }

    template <typename Iterator,
              typename Span,
              typename... Trs,
              typename = std::enable_if_t<!KuIsCompactView<Span, Trs...>::value
                                          && !KuIsView<Iterator>::value>>
    auto operator()(Iterator outputIterator, const KuView<Span, Trs...> &src) {
        using V = KuView<Span, Trs...>;
        using OutputType = typename std::iterator_traits<Iterator>::value_type;
        auto size = src.extents().prod();
        auto first = makeKuViewInputIterator(src, ku_size_t(0));
        auto last = makeKuViewInputIterator(src, ku_size_t(size));
        auto resultItr = algo::copy(m_device, outputIterator, first, last);
        KU_ASSERT((resultItr - outputIterator) == static_cast<std::ptrdiff_t>(size),
                  "copy count must match extents product");
        return src.extents();
    }

private:
    KuVendorContext<Vendor> m_device;
};

KuCopyFn(KuContext &) -> KuCopyFn<vendor::KuVendor>;

} // namespace kuai
