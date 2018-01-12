
#ifndef _TA_COMMON_WISEINT_H_
#define _TA_COMMON_WISEINT_H_ 1
#pragma once

class WiseInt {
public:
	WiseInt();

	inline int operator+(const int & a){
        m_int += a;
        return m_int;
    }
	inline int operator++(int) {	//ƥ���++
        return m_int++;
    }
	//int operator++() {	//ƥ��ǰ++

	operator int() const {
        return m_int;
    }

protected:
	int m_int;
};

#endif