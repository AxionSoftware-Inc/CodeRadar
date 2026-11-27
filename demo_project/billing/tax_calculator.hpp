#ifndef TAX_CALCULATOR_HPP
#define TAX_CALCULATOR_HPP

// Complex tax calculator in billing module
double compute_tax_total(double amount, double discount, int region) {
    double taxable_sum = amount - discount;
    if (taxable_sum < 0.0) {
        taxable_sum = 0.0;
    }
    double tax_rate = 0.12;
    if (region == 1) {
        tax_rate = 0.15;
    } else if (region == 2) {
        tax_rate = 0.20;
    } else if (region == 3) {
        tax_rate = 0.08;
    }
    double final_tax = taxable_sum * tax_rate;
    if (final_tax > 5000.0) {
        final_tax = final_tax * 0.95;
    }
    return final_tax;
}

#endif
