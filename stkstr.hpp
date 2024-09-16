/*
 * Copyright 2017 - 2021 Justas Masiulis
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

#ifndef JM_STKSTR_HPP
#define JM_STKSTR_HPP

#include <cstdint>
#include <cstddef>
#include <utility>
#include <type_traits>

#define stkstr(str) ::jm::xor_string([]() { return str; }, std::integral_constant<std::size_t, sizeof(str) / sizeof(*str)>{}, std::make_index_sequence<::jm::detail::_buffer_size<sizeof(str)>()>{})
#define stkstr_(str) stkstr(str).get()

#ifdef _MSC_VER
#define STKSTR_FORCEINLINE __forceinline
#else
#define STKSTR_FORCEINLINE __attribute__((always_inline)) inline
#endif

namespace jm {

    namespace detail {

        template<std::size_t Size>
        STKSTR_FORCEINLINE constexpr std::size_t _buffer_size()
        {
            return (Size / 8) + (Size % 8 != 0);
        }

        template<std::size_t S>
        STKSTR_FORCEINLINE constexpr std::uint64_t key8()
        {
            return 0;
        }

        // loads up to 8 characters of string into uint64 and xors it with the key
        template<std::size_t N, class CharT>
        STKSTR_FORCEINLINE constexpr std::uint64_t
        load_xored_str8(std::uint64_t key, std::size_t idx, const CharT* str) noexcept
        {
            using cast_type = typename std::make_unsigned<CharT>::type;
            constexpr auto value_size = sizeof(CharT);
            constexpr auto idx_offset = 8 / value_size;

            std::uint64_t value = 0;
            for(std::size_t i = 0; i < idx_offset && i + idx * idx_offset < N; ++i)
                value |=
                    (std::uint64_t{ static_cast<cast_type>(str[i + idx * idx_offset]) }
                     << ((i % idx_offset) * 8 * value_size));

            return value;
        }

        // forces compiler to use registers instead of stuffing constants in rdata
        STKSTR_FORCEINLINE std::uint64_t load_from_reg(std::uint64_t value) noexcept
        {
#if defined(__clang__) || defined(__GNUC__)
            asm("" : "=r"(value) : "0"(value) :);
            return value;
#else
            volatile std::uint64_t reg = value;
            return reg;
#endif
        }

    } // namespace detail

    template<class CharT, std::size_t Size, class Keys, class Indices>
    class xor_string;

    template<class CharT, std::size_t Size, std::uint64_t... Keys, std::size_t... Indices>
    class xor_string<CharT, Size, std::integer_sequence<std::uint64_t, Keys...>, std::index_sequence<Indices...>> {
        std::uint64_t _storage[sizeof...(Keys)];

    public:
        using value_type    = CharT;
        using size_type     = std::size_t;
        using pointer       = CharT*;
        using const_pointer = const CharT*;

        template<class L>
        STKSTR_FORCEINLINE xor_string(L l, std::integral_constant<std::size_t, Size>, std::index_sequence<Indices...>) noexcept
            : _storage{ ::jm::detail::load_from_reg((std::integral_constant<std::uint64_t, detail::load_xored_str8<Size>(Keys, Indices, l())>::value))... }
        {}

        STKSTR_FORCEINLINE constexpr size_type size() const noexcept
        {
            return Size - 1;
        }

        STKSTR_FORCEINLINE pointer get() noexcept
        {
            return (pointer)(_storage);
        }
    };

    template<class L, std::size_t Size, std::size_t... Indices>
    xor_string(L l, std::integral_constant<std::size_t, Size>, std::index_sequence<Indices...>) -> xor_string<
                std::remove_const_t<std::remove_reference_t<decltype(l()[0])>>,
                Size,
                std::integer_sequence<std::uint64_t, detail::key8<Indices>()...>,
                std::index_sequence<Indices...>>;

} // namespace jm

#endif // include guard
