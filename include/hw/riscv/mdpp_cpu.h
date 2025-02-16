/*
 * MDPP CPU types
 *
 * Copyright (c) 2017 MDPP, Inc.
 * Copyright (c) 2019 Bin Meng <bmeng.cn@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_MDPP_CPU_H
#define HW_MDPP_CPU_H

#if defined(TARGET_RISCV32)
#define MDPP_E_CPU TYPE_RISCV_CPU_MDPP_E31
#define MDPP_U_CPU TYPE_RISCV_CPU_MDPP_U34
#elif defined(TARGET_RISCV64)
#define MDPP_E_CPU TYPE_RISCV_CPU_MDPP_E51
#define MDPP_U_CPU TYPE_RISCV_CPU_MDPP_U54
#endif

#endif /* HW_MDPP_CPU_H */
