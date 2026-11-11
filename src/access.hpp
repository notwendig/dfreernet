#pragma once
namespace rnet {
enum class AccessLevel : unsigned { Therapist=0, Dealer=1, OEM=2, Factory=3 };
constexpr const char* access_level_name(AccessLevel level){
    switch(level){
    case AccessLevel::Therapist:return "Therapist";
    case AccessLevel::Dealer:return "Dealer";
    case AccessLevel::OEM:return "OEM";
    case AccessLevel::Factory:return "Factory";
    }
    return "Unknown";
}
constexpr bool access_allows(unsigned required,AccessLevel selected){
    return unsigned(selected)<=unsigned(AccessLevel::Factory)&&required<=unsigned(selected);
}
}
