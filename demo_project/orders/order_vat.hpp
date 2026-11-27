#ifndef ORDER_VAT_HPP
#define ORDER_VAT_HPP

// Semantic duplicate of tax_calculator in orders module!
// Uses different variable names, but identical algorithm structure & logic!
double calculate_order_vat(double base_sum, double rebate, int state_code) {
    double taxable_sum = base_sum - rebate;
    if (taxable_sum < 0.0) {
        taxable_sum = 0.0;
    }
    double tax_rate = 0.12;
    if (state_code == 1) {
        tax_rate = 0.15;
    } else if (state_code == 2) {
        tax_rate = 0.20;
    } else if (state_code == 3) {
        tax_rate = 0.08;
    }
    double final_tax = taxable_sum * tax_rate;
    if (final_tax > 5000.0) {
        final_tax = final_tax * 0.95;
    }
    return final_tax;
}

// Useless pass-through wrapper
void order_gateway_proxy(int id) {
    create_order(id);
}

#endif
