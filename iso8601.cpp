
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

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "iso8601.h"
#include "date.h"
#include "tz.h"
#include "iso_week.h"
#include <iostream>
#include <ctype.h>
#include <stdio.h>

namespace date {

  namespace detail {

    struct Helper
    {
      Helper()
        : _local(nullptr)
      {
        try
        {
          _local = date::current_zone();
        }
        catch(const std::exception& e_)
        {
          std::cerr << "ERROR: Cannot get current zone: " << e_.what() << "; fall back to use GMT instead!" << std::endl;
          _local = date::locate_zone("UTC");
        }
      }
      const date::time_zone* _local;
    };

    constexpr unsigned daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  } // detail

  const date::time_zone* zone_cache::utc()
  {
    static const date::time_zone* _utc = date::locate_zone("UTC");
    return _utc;
  }
  const date::time_zone* zone_cache::local()
  {
    // FIXME: arguable. If we do not want this we could just remove Helper class and call date::current_zone
    static const detail::Helper helper;
    return helper._local;
  }


namespace {

  struct FormatHelper
  {
    ISO8601Format::DateTime _date;
    ISO8601Format::DateTime _time;
    const char* _format_str_sep;
    const char* _format_str; 
  };

  static FormatHelper formats[] =
  {
    // date and time
    { ISO8601Format::DateTime::ymd,     ISO8601Format::DateTime::hms,     nullptr,          nullptr }, //  ymd_hms = 0; 
    { ISO8601Format::DateTime::ywd,     ISO8601Format::DateTime::hms,     nullptr,          nullptr }, // ywd_hms,
    { ISO8601Format::DateTime::yd,      ISO8601Format::DateTime::hms,     nullptr,          nullptr }, // yd_hms
    { ISO8601Format::DateTime::ymd,     ISO8601Format::DateTime::hm,      nullptr,          nullptr }, //  ymd_hm, 
    { ISO8601Format::DateTime::ywd,     ISO8601Format::DateTime::hm,      nullptr,          nullptr }, //  ywd_hm, 
    { ISO8601Format::DateTime::yd,      ISO8601Format::DateTime::hm,      nullptr,          nullptr }, //  yd_hm,
    { ISO8601Format::DateTime::ymd,     ISO8601Format::DateTime::h,       nullptr,          nullptr }, //  ymd_h
    { ISO8601Format::DateTime::ywd,     ISO8601Format::DateTime::h,       nullptr,          nullptr }, //  ywd_h,
    { ISO8601Format::DateTime::yd,      ISO8601Format::DateTime::h,       nullptr,          nullptr }, // yd_h,
    // time only
    { ISO8601Format::DateTime::Invalid, ISO8601Format::DateTime::hms,     "%02d:%02d:%02d", "%02d%02d%02d"},// hms
    { ISO8601Format::DateTime::Invalid, ISO8601Format::DateTime::hm,      "%02d:%02d",      "%02d%02d"},// hm
    { ISO8601Format::DateTime::Invalid, ISO8601Format::DateTime::h,       "%02d",           "%02d"},// h
    // date only
    { ISO8601Format::DateTime::ymd,     ISO8601Format::DateTime::Invalid, "%04d-%02d-%02d", "%04d%02d%02d" }, // ymd
    { ISO8601Format::DateTime::ywd,     ISO8601Format::DateTime::Invalid, "%04d-%c%02d-%d", "%04d%c%02d%d" }, // ywd,
    { ISO8601Format::DateTime::yd,      ISO8601Format::DateTime::Invalid, "%04d-%03d",      "%04d%03d" }, // yd,
    { ISO8601Format::DateTime::yw,      ISO8601Format::DateTime::Invalid, "%04d-%c%02d",    "%04d%c%02d" }, // yw,
    { ISO8601Format::DateTime::ym,      ISO8601Format::DateTime::Invalid, "%04d-%02d",      "%04d%02d" }, // ym,
    { ISO8601Format::DateTime::y,       ISO8601Format::DateTime::Invalid, "%04d",           "%04d" }, //y
  }; 

  inline size_t buflen(const char* p, const char* e)
  {
    if (p < e)
      return e - p;
    else
      return 0;
  }

  inline date::local_days today(const date::time_zone* zone_)
  {
    auto local_now = zone_->to_local(std::chrono::system_clock::now());
    return date::floor<date::days>(local_now);
  }

} // namespace 

namespace detail {

  size_t format_iso8601(char* buf_, size_t maxlen_, const ISO8601Format& format_, Precision precision_, 
      const date::local_time<std::chrono::nanoseconds>& lt_, std::chrono::seconds gmt_offset_)
  {
    using namespace std::chrono;

    if (!buf_)
      maxlen_ = 0;

    char* p = buf_;
    const char* e = buf_ + maxlen_;
    if (p < e)
      *p = 0;

    auto dtfmt = format_._datetime;

    if (dtfmt >= ISO8601Format::DateTime::Invalid)
    {
      return 0;
    }

    // get the local time. If format is zulu, get the sys_time but represent it in local_time.
    auto lt = lt_;
    if (format_._offset == ISO8601Format::Offset::zulu)
      lt -= gmt_offset_;
    auto ld = date::floor<date::days>(lt);
    date::time_of_day<nanoseconds> tod(lt - ld);

    const auto& fh = formats[(unsigned)dtfmt];
    if (fh._date != ISO8601Format::DateTime::Invalid)
    {
      const char* fmtstr = format_._extra._basicDatetime ? formats[(unsigned)fh._date]._format_str : formats[(unsigned)fh._date]._format_str_sep;
      switch (fh._date)
      {
        case ISO8601Format::DateTime::ywd:
        case ISO8601Format::DateTime::yw:
          {
            // get ISO weeknum and weekday
            iso_week::year_weeknum_weekday yww(ld);
            p += snprintf(p, buflen(p, e), fmtstr, static_cast<int>(yww.year()), 'W', static_cast<unsigned>(yww.weeknum()), static_cast<unsigned>(yww.weekday()));
            break;
          }
        case ISO8601Format::DateTime::yd:
          {
            date::year_month_day ymd(ld);
            int yd = ((ld - static_cast<date::local_days>(ymd.year()/1/1)) + date::days(1)).count();
            p += snprintf(p, buflen(p, e), fmtstr, static_cast<int>(ymd.year()), yd);
            break;
          }
        default:
          {
            // ymd / ym / y 
            date::year_month_day ymd(ld);
            p += snprintf(p, buflen(p, e), fmtstr, static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
            break;
          }
      }
    }

    if (fh._time != ISO8601Format::DateTime::Invalid)
    {
      // Now time part
      if (!format_._extra._omitT)
      {
        if (p < e)
          *p = 'T';
        ++p;
      }
      const char* fmtstr = format_._extra._basicDatetime ? formats[(unsigned)fh._time]._format_str : formats[(unsigned)fh._time]._format_str_sep;
      // assumes the Duration is at least second precision 
      int h = tod.hours().count();
      int m = tod.minutes().count();
      int s = tod.seconds().count();
      p += snprintf(p, buflen(p, e), fmtstr, h, m, s);

      // now fraction.. assuming it is ms/us/ns although not difficult to support picosecond
      auto frac_time = lt - date::floor<seconds>(lt);
      int64_t frac;
      int fdig = 0; 
      switch (fh._time)
      {
        case ISO8601Format::DateTime::hm:
          frac_time += tod.seconds();
          frac = frac_time.count() * 1000 / 60;
          fdig += 3;
          break;
        case ISO8601Format::DateTime::h:
          frac_time += tod.minutes() + tod.seconds();
          frac = frac_time.count() * 1000 * 1000 / 3600;
          fdig += 6;
          break;
        default:
          frac = frac_time.count();
          break;
      }
      int subsec_digit_num = 0;
      switch (precision_)
      {
        case Precision::Second:
          subsec_digit_num = 0;
          frac /= 1000000000;
          break;
        case Precision::MilliSecond:
          subsec_digit_num = 3;
          frac /= 1000000;
          break;
        case Precision::MicroSecond:
          subsec_digit_num = 6;
          frac /= 1000;
          break;
        case Precision::NanoSecond:
          subsec_digit_num = 9;
          break;
        default:
          break;
      }
      fdig += subsec_digit_num;

      // Make it a bit "intelligent: use the number of frac digits according to the time format and the precision of the duration
      // If hms, ms: 3 digits; us: 6 digits; ns: 9 digits
      // if hm:  ms: 6 digits; us: 9 digits; ns: 12 digits;
      // if h:   ms: 9 digits; us: 12 digits; ns: 15 digits
      if (frac)
      {
        // output the fraction part
        if (p < e)
        {
          *p = format_._extra._useComma ? ',' : '.';
        }
        ++p;
#ifdef _WIN32
        p += snprintf(p, buflen(p, e), "%0*I64d", fdig, frac);
#else
        p += snprintf(p, buflen(p, e), "%0*" PRId64, fdig, frac);
#endif
      }

      const char* tzfmt = format_._extra._basicOffset ? "%c%02d%02d" : "%c%02d:%02d";
      auto offset_format = gmt_offset_.count() == 0 && format_._offset != ISO8601Format::Offset::none ? ISO8601Format::Offset::zulu : format_._offset;
      switch (offset_format)
      {
        case ISO8601Format::Offset::zulu:
          p += snprintf(p, buflen(p, e), "Z");
          break;
        case ISO8601Format::Offset::h:
          tzfmt = "%c%02d";
          // fall through
        case ISO8601Format::Offset::hm:
          {
            auto offset = gmt_offset_.count();
            int min_offset = offset / 60;
            int hour_offset = min_offset / 60;
            char sign;
            if (min_offset < 0)
            {
              sign = '-';
              min_offset = hour_offset * 60 - min_offset;
              hour_offset = -hour_offset;
            }
            else
            {
              sign = '+';
              min_offset -= hour_offset * 60;
            }
            p += snprintf(p, buflen(p, e), tzfmt, sign, hour_offset, min_offset);
            break;
          }
        case ISO8601Format::Offset::none:
          break;
        default:
          return 0;
      }
    }

    return p - buf_;
  }
} // namespace detail


namespace {

  static const uint64_t frac_to_nano[] =
  {
    100000000000000,
    10000000000000,
    1000000000000,

    100000000000,
    10000000000,
    1000000000,
    100000000,

    10000000,
    1000000,
    100000,
    10000,

    1000,
    100,
    10,
    1
  };

  static int digits(const char* p, const char* e)
  {
    int count = 0;

    while (p < e && isdigit(*p))
    {
      count++;
      p++;
    }

    return count;
  }


  // Need have certain level of tolerance to make the format detection easier because it could have no T between date and time
#define RET_FMT(f) { fmt._datetime = ISO8601Format::DateTime::f; outFmt_ = fmt; return true; }
  static bool detectDateOrTimeFormat(const char* p, const char* e, ParseType pt_, ISO8601Format& outFmt_)
  {
    ISO8601Format fmt;

    // As the offset part is not detected here, we set the offset to NONE by default because we do not know whether there is offset part or not
    fmt._offset = ISO8601Format::Offset::none;

    bool time_only = (pt_ == ParseType::TimeOnly);
    if (p < e)
    {
      if (*p == 'T' || *p == 't')
      {
        if (pt_ == ParseType::DateOnly)
          return false;

        time_only = true;
        ++p;
      }
      else
      {
        //set omitT to true so that we do not need set this for the time only format. But we will set it back to false for the date format
        fmt._extra._omitT = true;
      }
    }

    int d = digits(p, e);
    p += d;

    switch (d)
    {
      case 2:
        // hh
        if (pt_ == ParseType::DateOnly)
          return false;

        if (p < e && *p == ':')
        {
          // hh:
          d = digits(++p, e);
          p += d;

          switch (d)
          {
            // hh:mm
            case 2:
              // hh:mm:
              if (p < e && *p == ':')
              {
                // hh:mm:ss
                if (digits(p + 1, e) == 2)
                {
                  RET_FMT(hms)
                }
                else
                  return false;
              }
              else
              {
                RET_FMT(hm)
              }
              break;

            default: return false;
          }
        }
        else
        {
          RET_FMT(h)
        }
        break;

      case 4:
        // hhmm
        if (time_only)
        {
          fmt._extra._basicDatetime = true;
          RET_FMT(hm)
        }

        // Because below returning date part format, there is no T.
        fmt._extra._omitT = false;

        // YYYY
        if (p < e && *p == '-')
        {
          // YYYY-
          d = digits(++p, e);
          p += d;

          switch (d)
          {
            case 0:
              if (p < e && (*p == 'W' || *p == 'w'))
              {
                // YYYY-W
                d = digits(++p, e);
                p += d;

                if (d == 2)
                {
                  if (p < e && *p == '-')
                  {
                    // YYYY-Www-
                    if (digits(p + 1, e) == 0)
                      return false;
                    // 1 or more digits because it is permitted to omit the T by mutual agreement. 
                    RET_FMT(ywd)
                  }
                  else
                  {
                    RET_FMT(yw)
                  }
                }
                else
                  return false;
              }
              else
              {
                return false;
              }
              break;

            case 2:
              // YYYY-MM
              if (p < e && *p == '-')
              {
                // YYYY-MM-
                switch (digits(p + 1, e))
                {
                  case 0:
                  case 1:
                    return false;
                  case 2:
                  default:
                    RET_FMT(ymd)
                }
              }
              else
              {
                RET_FMT(ym)
              }
              break;
            // YYYY-ddd
            case 3:
            default: 
              RET_FMT(yd)
          }
        }
        else if (p < e && (*p == 'W' || *p == 'w'))
        {
          fmt._extra._basicDatetime = true;
          // YYYYW
          switch (digits(p + 1, e))
          {
            // YYYYWww
            case 2:
              {
                RET_FMT(yw)
              }
            // YYYYWwwd
            case 3:
            default: 
              {
                RET_FMT(ywd)
              }
          }
        }
        else
        {
          RET_FMT(y)
        }
        break;

      case 6:
        {
          // YYYYMM is NOT allowed therefore this can only be time
          fmt._extra._basicDatetime = true;
          RET_FMT(hms)
          break;
        }
      case 7:
        {
          if (time_only)
            return false;

          // YYYYDDD
          fmt._extra._omitT = false;
          fmt._extra._basicDatetime = true;
          RET_FMT(yd)
          break;
        }
      case 8:
        {
          if (time_only)
            return false;

          // YYYYMMDD
          fmt._extra._basicDatetime = true;
          fmt._extra._omitT = false;
          RET_FMT(ymd)
            break;
        }
      default:
        {
          if (time_only)
            return false;

          fmt._extra._basicDatetime = true;
          if (d > 8)
          {
            // Because the number of digits of time part is even
            if (d & 1)
            {
              RET_FMT(yd)
            }
            else
            {
              RET_FMT(ymd)
            }
          }
          break;
        }
    }
    return false;
  }

  template <typename IntType>
    static int num(IntType* v, const char* p, int len)
    {
      IntType tv = 0;
      const char* e = p + len;
      while (p < e)
      {
        tv = tv * 10 + (*p - '0');
        p++;
      }

      *v = tv;

      return len;
    }

  static int frac(int64_t* v, const char* p, const char* e)
  {
    int d = digits(p, e);

    *v = 0;
    if( d > 0 )
    {
      int n = d > 15 ? 15 : d;

      num(v, p, n);
      *v *= frac_to_nano[n - 1];
    }

    return d;
  }

  // Get the local_days. We could get year_month_day, but in a lot of cases we will conver them into serial time (local_days)
  // Therefore we just make it return local_days directly and this could save some conversion efforts in the ywd or yw case.
  static const char* parseDate(const char* p, const ISO8601Format& fmt_, date::local_days& ld_)
  {
    using namespace date;
    const char* origP = p;

    int sep = !fmt_._extra._basicDatetime;

    switch (fmt_._datetime)
    {
      case ISO8601Format::DateTime::ym:
        {
          int y, m;
          p += num(&y, p, 4) + sep;
          p += num(&m, p, 2);
          auto ymd = date::year_month_day{date::year(y), date::month(m), date::day(1)};
          if (!ymd.ok())
            return origP;
          ld_ = static_cast<date::local_days>(ymd);
          break;
        }
      case ISO8601Format::DateTime::ymd:
        {
          int y, m, d;
          p += num(&y, p, 4) + sep;
          p += num(&m, p, 2) + sep;
          p += num(&d, p, 2);
          auto ymd = date::year_month_day{date::year(y), date::month(m), date::day(d)};
          if (!ymd.ok())
            return origP;
          ld_ = static_cast<date::local_days>(ymd);
          break;
        }
      case ISO8601Format::DateTime::yw:
        {
          int y, w;
          p += num(&y, p, 4) + (sep + 1);
          p += num(&w, p, 2);
          if (w < 1 || w > 53)
            return origP;

          iso_week::year_weeknum_weekday yww{iso_week::year(y), iso_week::weeknum(w), iso_week::weekday(1u)};
          ld_ = static_cast<date::local_days>(yww);
          break;
        }
      case ISO8601Format::DateTime::ywd:
        {
          int y, wk, wd;
          p += num(&y, p, 4) + (sep + 1);
          p += num(&wk, p, 2) + sep;
          p += num( &wd, p, 1 );
          iso_week::year_weeknum_weekday yww{iso_week::year(y), iso_week::weeknum(wk), iso_week::weekday(unsigned(wd))};
          if (!yww.ok())
            return origP;
          ld_ = static_cast<date::local_days>(yww);
          break;
        }
      case ISO8601Format::DateTime::yd:
        {
          int y, yd;
          p += num(&y, p, 4) + sep;
          p += num(&yd, p, 3);
          if (yd <= 0 || yd > (365 + date::year(y).is_leap()))
              return origP;
          ld_ = static_cast<date::local_days>(date::year(y) / 1 / 1) + date::days{yd - 1};
          break;
        }
      case ISO8601Format::DateTime::y:
        {
          int y;
          p += num(&y, p, 4);
          ld_ = static_cast<date::local_days>(date::year_month_day(date::year(y), date::month(1), date::day(1)));
          break;
        }
      default:
        return origP;
    }

    return p;
  }

  // Before parsing we do not know the subsecond precision, we have to assume one precision here. Nano second is usually good enough. 
  // Time includes offset information thus we do not name it as parseTimeAndOffset. 
  static const char* parseTime(const char* p, const char* e, ISO8601Format& fmt_, std::chrono::nanoseconds& since_midnight_, std::chrono::seconds& gmt_offset_)
  {
    using namespace std::chrono;
    const char* origP = p;

    if (!fmt_._extra._omitT)
      p++;
    int sep = !fmt_._extra._basicDatetime;

    int64_t fsec;
    since_midnight_ = nanoseconds{0};
    switch (fmt_._datetime)
    {
      case ISO8601Format::DateTime::h:
        {
          int h;
          p += num(&h, p, 2);
          since_midnight_ = hours(h);
          fsec = 60 * 60;
          break;
        }
      case ISO8601Format::DateTime::hm:
        {
          int h, m;
          p += num(&h, p, 2) + sep;
          p += num(&m, p, 2);
          since_midnight_ = hours(h) + minutes(m);
          fsec = 60;
          break;
        }
      case ISO8601Format::DateTime::hms:
        {
          int h, m, s;
          p += num(&h, p, 2) + sep;
          p += num(&m, p, 2) + sep;
          p += num(&s, p, 2);
          since_midnight_ = hours(h) + minutes(m) + seconds(s);
          fsec = 1;
          break;
        }
      default:
        assert(false);
        return origP;
    }

    if (p < e)
    {
      bool frComma = (',' == *p);

      if (('.' == *p) || frComma)
      {
        fmt_._extra._useComma = frComma;

        ++p;
        int64_t v;
        int fdig = frac(&v, p, e);
        if( fdig == 0 )
          return origP;
        p += fdig;

        int64_t ns = v * fsec / 1000000;
        since_midnight_ += nanoseconds(ns);
      }
    }

    if (p < e)
    {
      bool negOffs = false;
      switch(*p)
      {
        case 'z':
        case 'Z':
          ++p;
          fmt_._offset = ISO8601Format::Offset::zulu;
          gmt_offset_ = seconds{0};
          break;
        case '-':
          negOffs = true;
          // fall through
        case '+':
          // gmt offset
          {
            int hr = 0, mn = 0;

            switch (digits(++p, e))
            {
              case 2:
                p += num(&hr, p, 2);

                if (p < e && *p == ':')
                {
                  fmt_._extra._basicOffset = false;
                  if (digits(++p, e) == 2)
                  {
                    p += num(&mn, p, 2);
                    fmt_._offset = ISO8601Format::Offset::hm;
                  }
                  else
                    return origP;
                }
                else
                  fmt_._offset = ISO8601Format::Offset::h;
                break;

              case 4:
                fmt_._extra._basicOffset = true;

                p += num(&hr, p, 2);
                p += num(&mn, p, 2);

                fmt_._offset = ISO8601Format::Offset::hm;
                break;

              default:
                return origP;

            }

            gmt_offset_ = hours(hr) + minutes(mn);
            if (negOffs)
              gmt_offset_ = -gmt_offset_;
          }

          break;
        default:
          fmt_._offset = ISO8601Format::Offset::none;
          gmt_offset_ = seconds{0};
          break;
      }
    }
    else
    {
      fmt_._offset = ISO8601Format::Offset::none;
      gmt_offset_ = seconds{0};
    }

    return p;
  }

  // first dimension is DATE: ymd: 0, ywd: 1, yd: 2
  // second dimension is TIME: hms: 0, hm: 1, h: 2
  ISO8601Format::DateTime combine_formats[3][3] = {
    { ISO8601Format::DateTime::ymd_hms, ISO8601Format::DateTime::ymd_hm, ISO8601Format::DateTime::ymd_h },
    { ISO8601Format::DateTime::ywd_hms, ISO8601Format::DateTime::ywd_hm, ISO8601Format::DateTime::ywd_h },
    { ISO8601Format::DateTime::yd_hms,  ISO8601Format::DateTime::yd_hm,  ISO8601Format::DateTime::yd_h  }
  };
} // namespace

namespace detail {

  std::pair<size_t, ISO8601Format> parse_iso8601(const char* str_, size_t len_,
      date::local_days& ld_, std::chrono::nanoseconds& since_midnight_,
      std::chrono::seconds& gmt_offset_, ParseType pt_)
  {
    ISO8601Format format;
    const char* e = str_ + len_;
    gmt_offset_ = std::chrono::seconds{0};
    since_midnight_ = std::chrono::nanoseconds{0};

    switch (pt_)
    {
      case ParseType::DateOnly:
        {
          if (!detectDateOrTimeFormat(str_, e, pt_, format))
            return std::make_pair(0, format);
          const char* p = parseDate(str_, format, ld_);
          return std::make_pair(p - str_, format);
        }
      case ParseType::TimeOnly:
        {
          if (!detectDateOrTimeFormat(str_, e, pt_, format))
            return std::make_pair(0, format);
          const char* p = parseTime(str_, e, format, since_midnight_, gmt_offset_);
          ld_ = today(zone_cache::local());
          return std::make_pair(p - str_, format);
        }
      case ParseType::DateTime:
      default:
        {
          if (!detectDateOrTimeFormat(str_, e, ParseType::DateTime, format))
            return std::make_pair(0, format);

          const char* p = str_;
          if (format._datetime >= ISO8601Format::DateTime::FirstTimeOnly && format._datetime <= ISO8601Format::DateTime::LastTimeOnly)
          {
            // No date part, then just parse time and return
            p = parseTime(str_, e, format, since_midnight_, gmt_offset_);
            ld_ = today(zone_cache::local());
            return std::make_pair(p - str_, format);
          }

          p = parseDate(str_, format, ld_);
          // if cannot parse a date then failed
          if (p == str_)
            return std::make_pair(0, format);

          auto dateFormat = format._datetime;
          bool basicDatetime = format._extra._basicDatetime;

          if (dateFormat >= ISO8601Format::DateTime::ymd && 
              dateFormat <= ISO8601Format::DateTime::yd &&
              detectDateOrTimeFormat(p, e, ParseType::TimeOnly, format))
          {
            p = parseTime(p, e, format, since_midnight_, gmt_offset_);
            // Now merge the format with dateFormat and basicDatetime
            if (format._datetime == ISO8601Format::DateTime::h)
            {
              // in this case do not care about the discrepancy between the basic datetime
              format._extra._basicDatetime = basicDatetime;
            }
            else if (basicDatetime != format._extra._basicDatetime)
            {
              // it means one of the date and time parts use basic format but the other does not. This is not valid
              return std::make_pair(0, format);
            }
            auto dateidx = (uint8_t)dateFormat - (uint8_t)ISO8601Format::DateTime::ymd;
            auto timeidx = (uint8_t)format._datetime - (uint8_t)ISO8601Format::DateTime::hms;
            format._datetime = combine_formats[dateidx][timeidx];
          }

          return std::make_pair(p - str_, format);
        }
    }
  }
} // namespace detail

} // namespace date
