#include "parameter.h"

namespace scheduling_problem
{
    /* param value */
    /**
     * Constructor
     */
    parameter::param_value::param_value() : string(nullptr) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(unsigned u_number) : u_number(u_number) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(int number) : number(number) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(double number_float) : number_float(number_float) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(bool boolean) : boolean(boolean) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(const std::string &s) : string(new std::string(s)) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(const char *s) : param_value(std::string(s)) {}
    /**
     * Constructor
     */
    parameter::param_value::param_value(value_t t) : string(nullptr)
    {
        switch (t)
        {
        case value_t::number_integer_unsigned:
            u_number = static_cast<unsigned>(0);
            break;
        case value_t::number_integer:
            number = 0;
            break;
        case value_t::number_float:
            number_float = 0.0;
            break;
        case value_t::boolean:
            boolean = false;
            break;
        case value_t::string:
            string = new std::string();
            break;
        default:
            break;
        }
    }
    /**
     * Free the space
     */
    void parameter::param_value::destroy(value_t t)
    {
        switch (t)
        {
        case value_t::string:
            delete string;
            break;
        default:
            break;
        }
    }
    /**
     * Destructor
     */
    parameter::param_value::~param_value()
    {
    }

    /* parameter */

    parameter::parameter() : type_(value_t::object), value_() {}

    parameter::parameter(const param_value &value, value_t type) : type_(type)
    {
        switch (type_)
        {
        case value_t::number_integer_unsigned:
            value_.u_number = value.u_number;
            break;
        case value_t::number_integer:
            value_.number = value.number;
            break;
        case value_t::number_float:
            value_.number_float = value.number_float;
            break;
        case value_t::boolean:
            value_.boolean = value.boolean;
            break;
        case value_t::string:
            value_.string = new std::string(*value.string);
            break;
        default:
            break;
        }
    }

    parameter::parameter(const parameter &other)
    {
        type_ = other.type();
        switch (other.type())
        {
        case value_t::number_integer_unsigned:
            value_.u_number = other.value_.u_number;
            break;
        case value_t::number_integer:
            value_.number = other.value_.number;
            break;
        case value_t::number_float:
            value_.number_float = other.value_.number_float;
            break;
        case value_t::boolean:
            value_.boolean = other.value_.boolean;
            break;
        case value_t::string:
            value_.string = new std::string(*other.value_.string);
            break;
        default:
            break;
        }
    }

    parameter::parameter(int num) : parameter(param_value(num), value_t::number_integer) {}

    parameter::parameter(unsigned u_num) : parameter(param_value(u_num), value_t::number_integer_unsigned) {}

    parameter::parameter(double f_num) : parameter(param_value(f_num), value_t::number_float) {}

    parameter::parameter(bool b) : parameter(param_value(b), value_t::boolean) {}

    parameter::parameter(const std::string &s) : parameter(param_value(s), value_t::string) {}

    parameter::parameter(const char *s) : parameter(param_value(s), value_t::string) {}

    parameter parameter::operator=(const parameter &other)
    {
        value_.destroy(type_);
        type_ = other.type();
        switch (other.type())
        {
        case value_t::number_integer_unsigned:
            value_.u_number = other.value_.u_number;
            break;
        case value_t::number_integer:
            value_.number = other.value_.number;
            break;
        case value_t::number_float:
            value_.number_float = other.value_.number_float;
            break;
        case value_t::boolean:
            value_.boolean = other.value_.boolean;
            break;
        case value_t::string:
            value_.string = new std::string(*other.value_.string);
            break;
        default:
            break;
        }
        return *this;
    }

    parameter parameter::operator+(const parameter &other)
    {
        parameter sum = *this;
        if (type() == other.type())
        {
            switch (other.type())
            {
            case value_t::number_integer_unsigned:
                sum.value_.u_number += other.value_.u_number;
                break;
            case value_t::number_integer:
                sum.value_.number += other.value_.number;
                break;
            case value_t::number_float:
                sum.value_.number_float += other.value_.number_float;
                break;
            case value_t::string:
                sum.value_.string->operator+=(*other.value_.string);
                break;
            default:
                break;
            }
        }
        return sum;
    }

    parameter parameter::operator-(const parameter &other)
    {
        parameter sum = *this;
        if (type() == other.type() && type() != value_t::string)
        {
            switch (other.type())
            {
            case value_t::number_integer_unsigned:
                sum.value_.u_number -= other.value_.u_number;
                break;
            case value_t::number_integer:
                sum.value_.number -= other.value_.number;
                break;
            case value_t::number_float:
                sum.value_.number_float -= other.value_.number_float;
                break;
            default:
                break;
            }
        }
        return sum;
    }

    parameter parameter::operator*(const parameter &other)
    {
        parameter sum = *this;
        if (type() == other.type() && type() != value_t::string)
        {
            switch (other.type())
            {
            case value_t::number_integer_unsigned:
                sum.value_.u_number *= other.value_.u_number;
                break;
            case value_t::number_integer:
                sum.value_.number *= other.value_.number;
                break;
            case value_t::number_float:
                sum.value_.number_float *= other.value_.number_float;
                break;
            default:
                break;
            }
        }
        return sum;
    }

    parameter parameter::operator+=(const parameter &other)
    {
        *this = *this + other;
        return *this;
    }

    parameter parameter::operator-=(const parameter &other)
    {
        *this = *this - other;
        return *this;
    }

    parameter parameter::operator*=(const parameter &other)
    {
        *this = *this * other;
        return *this;
    }

    parameter::operator unsigned() const
    {
        unsigned res(0);
        switch (type())
        {
        case value_t::number_integer:
            res = unsigned(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = unsigned(value_.u_number);
            break;
        case value_t::number_float:
            res = unsigned(value_.number_float);
            break;
        case value_t::boolean:
            res = unsigned(value_.boolean);
            break;
        default:
            break;
        }
        return res;
    }

    parameter::operator double() const
    {
        double res(0);
        switch (type())
        {
        case value_t::number_integer:
            res = double(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = double(value_.u_number);
            break;
        case value_t::number_float:
            res = double(value_.number_float);
            break;
        case value_t::boolean:
            res = double(value_.boolean);
            break;
        default:
            break;
        }
        return res;
    }

    parameter::operator bool() const
    {
        bool res(false);
        switch (type())
        {
        case value_t::number_integer:
            res = bool(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = bool(value_.u_number);
            break;
        case value_t::number_float:
            res = bool(value_.number_float);
            break;
        case value_t::boolean:
            res = value_.boolean;
            break;
        default:
            break;
        }
        return res;
    }

    parameter::operator std::string() const
    {
        std::string res;
        switch (type())
        {
        case value_t::number_integer:
            res = std::to_string(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = std::to_string(value_.u_number);
            break;
        case value_t::number_float:
            res = std::to_string(value_.number_float);
            break;
        case value_t::boolean:
            res = std::to_string(value_.boolean);
            break;
        case value_t::string:
            res = *value_.string;
            break;
        default:
            break;
        }
        return res;
    }

    value_t parameter::type() const
    {
        return type_;
    }

    parameter::~parameter()
    {
        value_.destroy(type_);
    }

    std::ostream &operator<<(std::ostream &out, const parameter &param)
    {
        switch (param.type())
        {
        case value_t::number_integer_unsigned:
            out << param.value_.number;
            break;
        case value_t::number_integer:
            out << param.value_.u_number;
            break;
        case value_t::number_float:
            out << param.value_.number_float;
            break;
        case value_t::boolean:
            out << param.value_.boolean;
            break;
        case value_t::string:
            out << *param.value_.string;
            break;
        case value_t::object:
            out << "";
        default:
            break;
        }
        return out;
    }
}
