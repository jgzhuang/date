#pragma once

// The MIT License (MIT)
//
// Copyright (c) 2016 Jiangang Zhuang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Our apologies.  When the previous paragraph was written, lowercase had not yet
// been invented (that woud involve another several millennia of evolution).
// We did not mean to shout.


#include "date.h"
#include "tz.h"
#include <string>


namespace date {
  struct zone_cache
  {
    public:
      /// Return the cached GMT (UTC) time zone pointer obtained from tz lib
      static const date::time_zone* utc();
      /// Return the cached local time zone pointer obtained from date::current_zone()
      static const date::time_zone* local();
  };

  struct ISO8601Format
  {
    /**
     * Controls which datetime fields will be included in the ISO8601 string
     */
    enum class DateTime: uint8_t
    {
      // date and time.
      ymd_hms= 0,  // default format for datetime
      ywd_hms,
      yd_hms,
      ymd_hm, 
      ywd_hm, 
      yd_hm,
      ymd_h,
      ywd_h,
      yd_h,
      // time only
      hms,
      hm,
      h, 
      // date only
      ymd,
      ywd,
      yd,
      yw,
      ym,
      y,

      Invalid,

      FirstTimeOnly = hms,
      LastTimeOnly = h,
      FirstDateOnly = ymd,
      LastDateOnly = y,
      FirstFormat = ymd_hms,
      LastFormat = y,
      FormatCount = Invalid
    };

    /**
     * Controls how to format the GMT offset. 
     */
    enum class Offset : uint8_t
    {
      hm = 0, // default format for offset as this is the most common case when formatting 
      h,
      zulu,   /// Z
      none,

      // make it easier for testing
      FirstFormat = hm,
      LastFormat = none 
    };

    /**
     * Extra format control. At the moment we do not allow user to specify the
     * number of digits for fraction. Millisecond/microsecond/nanosecond use 
     * 3/6/9 digits for fraction if second is presented. If only minute is
     * presented each adds 3 digits for fraction, and if only hour is presented
     * each addes 6 digits for fraction.
     * If there is a demand to specify the number of digits, we could use the 
     * reserved field. 
     */ 
    struct Extra
    {
      // FIXME: should we use comma by default? ISO8601:2004 prefers comma
      uint8_t _useComma : 1; // 1 means using comma as decimal mark, 0 means using dot as decimal mark
      uint8_t _omitT : 1;  // Whether timeonly will have leading T
      uint8_t _basicDatetime : 1; // Basic format with no separators
      uint8_t _basicOffset : 1; // Whether has separator in Offset type
      uint8_t _dummy : 4; 

      Extra() 
        : _useComma(0), _omitT(0), _basicDatetime(0), _basicOffset(0), _dummy(0)
      {
      }

      Extra(bool uc_, bool nlt_, bool bd_, bool bo_)
        : _useComma(uc_), _omitT(nlt_), _basicDatetime(bd_), _basicOffset(bo_), _dummy(0)
      {
      }
      
    };

    DateTime _datetime;
    Offset _offset;
    Extra _extra;
    uint8_t _reserved;

    ISO8601Format() 
      : _datetime(DateTime::ymd_hms), _offset(Offset::hm), _reserved(0)
    {
    }

    ISO8601Format(DateTime dt_, Offset o_, Extra ef_)
      : _datetime(dt_), _offset(o_), _extra(ef_), _reserved(0)
    {
    }

    ISO8601Format& offset(Offset offset_)
    {
      _offset = offset_;
      return *this;
    }
    ISO8601Format& useComma(bool uc_)
    {
      _extra._useComma = uc_;
      return *this;
    }
    ISO8601Format& basicDatetime(bool bd_)
    {
      _extra._basicDatetime = bd_;
      return *this;
    }
  };

  inline bool operator==(const ISO8601Format::Extra& lhs_, const ISO8601Format::Extra& rhs_)
  {
    return lhs_._useComma == rhs_._useComma && lhs_._omitT == rhs_._omitT 
      && lhs_._basicDatetime == rhs_._basicDatetime 
      && lhs_._basicOffset == rhs_._basicOffset && lhs_._dummy == rhs_._dummy;
  }

  inline bool operator==(const ISO8601Format& lhs_, const ISO8601Format& rhs_)
  {
    return (lhs_._datetime == rhs_._datetime && lhs_._offset == rhs_._offset
        && lhs_._extra == rhs_._extra);
  }


  /**
   * The expected data in the ISO8601 string to be parsed. Usually DateTime
   * is good enough to handle date only, time only and datetime strings. In
   * some cases there is ambiguity, for example, four digits could be hhmm 
   * or yyyy in basic datetime form. In this case specifying ParseType could
   * help isambiguate.
   */
  enum class ParseType {
    DateOnly = 1,
    TimeOnly = 2,
    DateTime = 3
  };  

  // Types and functions in detail namespace is for internal use only as we are
  // less concerned about changing their implementation and meaning.
  namespace detail {
    /**
     * Tells the precision of the time point to be formatted. So far up to 
     * nanosecond is supported.
     */
    enum class Precision
    {
      Second,
      MilliSecond,
      MicroSecond,
      NanoSecond,

      FirstPrecision = Second,
      LastPrecision = NanoSecond
    };

    /// Get the Precision enum value out of supported Duration types
    template <class Duration>
    constexpr Precision get_precision()
      {
        return 
          Duration{1} >= std::chrono::seconds{1} ? Precision::Second :
          Duration{1} >= std::chrono::milliseconds{1} ? Precision::MilliSecond :
          Duration{1} >= std::chrono::microseconds{1} ? Precision::MicroSecond : 
          Precision::NanoSecond;
      }

    // This function has the logic to do ISO8601 formatting and is called by
    // the public format_iso8601 function template. This could avoid generating
    // several copies of almost identical code for different Duration types. 
    // The nanosecond time point can accept any precion coarser than nanosecond
    // thus the Precision enum is used to tell the "original" precision.
    // At the moment we do not support adding the [tz_name] at the end, e.g. 
    // 2015-01-01T02:03:04-04:00[America/New_York].
    DATE_API size_t format_iso8601(char* buf_, size_t maxlen_, 
        const ISO8601Format& format_, Precision,
        const date::local_time<std::chrono::nanoseconds>& lt_, 
        std::chrono::seconds gmt_offset_);

    // Similar as format_iso8601, this function has the logic to do ISO8601
    // parsing and is called by the public parse_iso8601 function. 
    DATE_API std::pair<size_t, ISO8601Format> parse_iso8601(const char* str_, size_t len,
        date::local_days& ld_, std::chrono::nanoseconds& time_since_midnight_,
        std::chrono::seconds& gmt_offset_, ParseType pt_);
  } // namespace detail

  /**
   * @brief Formats a zoned time point with precision up to nanoseconds
   *        according to the format_.
   */
  template <class Duration>
  std::string format_iso8601(const date::zoned_time<Duration>& zt_, 
      const ISO8601Format& format_ = ISO8601Format{})
  {
    auto lt = zt_.get_local_time();
    auto si = zt_.get_info();
    // 64 bytes long is enough for ISO8601 format
    char buf[64];
    detail::format_iso8601(buf, sizeof(buf), format_, detail::get_precision<Duration>(), lt, si.offset);
    return std::string(buf);
  }
 
  /**
   * @brief Parses an ISO8601 string into local timepoint and extract the GMT offset if it has.
   * @return Returns a pair with first indicating the number of characters 
   *         processed and second indicating the format of the string
   */
  template <class Duration>
  std::pair<size_t, ISO8601Format> parse_iso8601(const char* str_, size_t len_,
      date::local_time<Duration>& ltp_, std::chrono::seconds& gmt_offset_,
      ParseType pt_ = ParseType::DateTime)
  {
    using namespace std::chrono;
    using namespace date;

    date::local_days ld;
    nanoseconds since_midnight;
    auto p = detail::parse_iso8601(str_, len_, ld, since_midnight, gmt_offset_, pt_);
    auto nsltp = ld + since_midnight;
    if (p.first > 0)
    {
      ltp_ = std::chrono::time_point_cast<Duration>(nsltp);
    }
    return p;
  }

  template <class Duration>
  std::pair<size_t, ISO8601Format> parse_iso8601(const std::string& str_,
      date::local_time<Duration>& ltp_, std::chrono::seconds& gmt_offset_,
      ParseType pt_ = ParseType::DateTime)
  {
    return parse_iso8601(str_.c_str(), str_.length(), ltp_, gmt_offset_, pt_);
  }

  /**
   * @brief Parses an ISO8601 string into system timepoint. 
   * If the ISO8601 string has neither GMT offset nor Z, it does NOT use local 
   */
  template <class Duration>
  std::pair<size_t, ISO8601Format> parse_iso8601(const char* str_, size_t len_,
      date::sys_time<Duration>& stp_, ParseType pt_ = ParseType::DateTime)
  {
    date::local_time<Duration> ltp;
    std::chrono::seconds gmt_offset;
    auto p = parse_iso8601(str_, len_, ltp, gmt_offset, pt_);
    if (p.first)
    {
      stp_ = date::sys_time<Duration>{ltp.time_since_epoch()} - gmt_offset;
    }
    return p;
  }

  template <class Duration>
  std::pair<size_t, ISO8601Format> parse_iso8601(const std::string& str_,
      date::sys_time<Duration>& stp_, ParseType pt_ = ParseType::DateTime)
  {
    return parse_iso8601(str_.c_str(), str_.length(), stp_, pt_);
  }


} // namespace date


