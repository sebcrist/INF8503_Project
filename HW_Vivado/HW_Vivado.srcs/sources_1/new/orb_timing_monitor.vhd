library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity orb_timing_monitor is
    port (
        s_axi_aclk    : in  std_logic;
        s_axi_aresetn : in  std_logic;
        s_axi_awaddr  : in  std_logic_vector(4 downto 0);
        s_axi_awvalid : in  std_logic;
        s_axi_awready : out std_logic;
        s_axi_wdata   : in  std_logic_vector(31 downto 0);
        s_axi_wstrb   : in  std_logic_vector(3 downto 0);
        s_axi_wvalid  : in  std_logic;
        s_axi_wready  : out std_logic;
        s_axi_bresp   : out std_logic_vector(1 downto 0);
        s_axi_bvalid  : out std_logic;
        s_axi_bready  : in  std_logic;
        s_axi_araddr  : in  std_logic_vector(4 downto 0);
        s_axi_arvalid : in  std_logic;
        s_axi_arready : out std_logic;
        s_axi_rdata   : out std_logic_vector(31 downto 0);
        s_axi_rresp   : out std_logic_vector(1 downto 0);
        s_axi_rvalid  : out std_logic;
        s_axi_rready  : in  std_logic;
        tap_in_vld    : in  std_logic;
        tap_in_rdy    : in  std_logic;
        tap_gray_vld  : in  std_logic;
        tap_gray_rdy  : in  std_logic;
        tap_gauss_vld : in  std_logic;
        tap_gauss_rdy : in  std_logic;
        tap_fast_vld  : in  std_logic;
        tap_fast_rdy  : in  std_logic;
        tap_brief_vld : in  std_logic;
        tap_brief_rdy : in  std_logic;
        tap_out_vld   : in  std_logic;
        tap_out_rdy   : in  std_logic;
        tap_out_lst   : in  std_logic
    );
end entity orb_timing_monitor;

architecture rtl of orb_timing_monitor is
    type u32_array_t is array (natural range <>) of unsigned(31 downto 0);

    signal tap_in_vld_r1    : std_logic := '0';
    signal tap_in_rdy_r1    : std_logic := '0';
    signal tap_gray_vld_r1  : std_logic := '0';
    signal tap_gray_rdy_r1  : std_logic := '0';
    signal tap_gauss_vld_r1 : std_logic := '0';
    signal tap_gauss_rdy_r1 : std_logic := '0';
    signal tap_fast_vld_r1  : std_logic := '0';
    signal tap_fast_rdy_r1  : std_logic := '0';
    signal tap_brief_vld_r1 : std_logic := '0';
    signal tap_brief_rdy_r1 : std_logic := '0';
    signal tap_out_vld_r1   : std_logic := '0';
    signal tap_out_rdy_r1   : std_logic := '0';
    signal tap_out_lst_r1   : std_logic := '0';

    signal tap_in_vld_r2    : std_logic := '0';
    signal tap_in_rdy_r2    : std_logic := '0';
    signal tap_gray_vld_r2  : std_logic := '0';
    signal tap_gray_rdy_r2  : std_logic := '0';
    signal tap_gauss_vld_r2 : std_logic := '0';
    signal tap_gauss_rdy_r2 : std_logic := '0';
    signal tap_fast_vld_r2  : std_logic := '0';
    signal tap_fast_rdy_r2  : std_logic := '0';
    signal tap_brief_vld_r2 : std_logic := '0';
    signal tap_brief_rdy_r2 : std_logic := '0';
    signal tap_out_vld_r2   : std_logic := '0';
    signal tap_out_rdy_r2   : std_logic := '0';
    signal tap_out_lst_r2   : std_logic := '0';

    signal evt_start        : std_logic := '0';
    signal evt_taps         : std_logic_vector(0 to 3) := (others => '0');
    signal evt_end          : std_logic := '0';

    signal active           : std_logic := '0';
    signal cycle_cnt        : unsigned(31 downto 0) := (others => '0');
    signal seen_taps        : std_logic_vector(0 to 3) := (others => '0');
    signal cur_taps         : u32_array_t(0 to 3) := (others => (others => '0'));

    signal reg_total        : unsigned(31 downto 0) := (others => '0');
    signal reg_taps         : u32_array_t(0 to 3) := (others => (others => '0'));
    signal reg_frames       : unsigned(31 downto 0) := (others => '0');

    signal awready_i        : std_logic := '0';
    signal wready_i         : std_logic := '0';
    signal bvalid_i         : std_logic := '0';
    signal arready_i        : std_logic := '0';
    signal rvalid_i         : std_logic := '0';
    signal rdata_i          : std_logic_vector(31 downto 0) := (others => '0');
begin
    s_axi_awready <= awready_i;
    s_axi_wready  <= wready_i;
    s_axi_bresp   <= "00";
    s_axi_bvalid  <= bvalid_i;
    s_axi_arready <= arready_i;
    s_axi_rdata   <= rdata_i;
    s_axi_rresp   <= "00";
    s_axi_rvalid  <= rvalid_i;

    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            tap_in_vld_r1    <= tap_in_vld;
            tap_in_rdy_r1    <= tap_in_rdy;
            tap_gray_vld_r1  <= tap_gray_vld;
            tap_gray_rdy_r1  <= tap_gray_rdy;
            tap_gauss_vld_r1 <= tap_gauss_vld;
            tap_gauss_rdy_r1 <= tap_gauss_rdy;
            tap_fast_vld_r1  <= tap_fast_vld;
            tap_fast_rdy_r1  <= tap_fast_rdy;
            tap_brief_vld_r1 <= tap_brief_vld;
            tap_brief_rdy_r1 <= tap_brief_rdy;
            tap_out_vld_r1   <= tap_out_vld;
            tap_out_rdy_r1   <= tap_out_rdy;
            tap_out_lst_r1   <= tap_out_lst;

            tap_in_vld_r2    <= tap_in_vld_r1;
            tap_in_rdy_r2    <= tap_in_rdy_r1;
            tap_gray_vld_r2  <= tap_gray_vld_r1;
            tap_gray_rdy_r2  <= tap_gray_rdy_r1;
            tap_gauss_vld_r2 <= tap_gauss_vld_r1;
            tap_gauss_rdy_r2 <= tap_gauss_rdy_r1;
            tap_fast_vld_r2  <= tap_fast_vld_r1;
            tap_fast_rdy_r2  <= tap_fast_rdy_r1;
            tap_brief_vld_r2 <= tap_brief_vld_r1;
            tap_brief_rdy_r2 <= tap_brief_rdy_r1;
            tap_out_vld_r2   <= tap_out_vld_r1;
            tap_out_rdy_r2   <= tap_out_rdy_r1;
            tap_out_lst_r2   <= tap_out_lst_r1;

            evt_start        <= tap_in_vld_r2 and tap_in_rdy_r2;
            evt_taps(0)      <= tap_gray_vld_r2 and tap_gray_rdy_r2;
            evt_taps(1)      <= tap_gauss_vld_r2 and tap_gauss_rdy_r2;
            evt_taps(2)      <= tap_fast_vld_r2 and tap_fast_rdy_r2;
            evt_taps(3)      <= tap_brief_vld_r2 and tap_brief_rdy_r2;
            evt_end          <= tap_out_vld_r2 and tap_out_rdy_r2 and tap_out_lst_r2;
        end if;
    end process;

    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                active     <= '0';
                cycle_cnt  <= (others => '0');
                seen_taps  <= (others => '0');
                cur_taps   <= (others => (others => '0'));
                reg_total  <= (others => '0');
                reg_taps   <= (others => (others => '0'));
                reg_frames <= (others => '0');
            else
                if active = '0' then
                    if evt_start = '1' then
                        active    <= '1';
                        cycle_cnt <= to_unsigned(1, 32);
                        seen_taps <= (others => '0');
                        cur_taps  <= (others => (others => '0'));
                    end if;
                else
                    cycle_cnt <= cycle_cnt + 1;

                    for i in 0 to 3 loop
                        if seen_taps(i) = '0' and evt_taps(i) = '1' then
                            seen_taps(i) <= '1';
                            cur_taps(i)  <= cycle_cnt;
                        end if;
                    end loop;

                    if evt_end = '1' then
                        reg_total  <= cycle_cnt;
                        reg_taps   <= cur_taps;
                        reg_frames <= reg_frames + 1;
                        active     <= '0';
                    end if;
                end if;
            end if;
        end if;
    end process;

    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                awready_i <= '0';
                wready_i  <= '0';
                bvalid_i  <= '0';
                arready_i <= '0';
                rvalid_i  <= '0';
                rdata_i   <= (others => '0');
            else
                if awready_i = '0' and wready_i = '0' and s_axi_awvalid = '1' and s_axi_wvalid = '1' and bvalid_i = '0' then
                    awready_i <= '1';
                    wready_i  <= '1';
                    bvalid_i  <= '1';
                else
                    awready_i <= '0';
                    wready_i  <= '0';
                    if bvalid_i = '1' and s_axi_bready = '1' then
                        bvalid_i <= '0';
                    end if;
                end if;

                if arready_i = '0' and s_axi_arvalid = '1' and rvalid_i = '0' then
                    arready_i <= '1';
                    rvalid_i  <= '1';
                    case s_axi_araddr(4 downto 2) is
                        when "000"  => rdata_i <= std_logic_vector(reg_total);
                        when "001"  => rdata_i <= std_logic_vector(reg_taps(0));
                        when "010"  => rdata_i <= std_logic_vector(reg_taps(1));
                        when "011"  => rdata_i <= std_logic_vector(reg_taps(2));
                        when "100"  => rdata_i <= std_logic_vector(reg_taps(3));
                        when "101"  => rdata_i <= std_logic_vector(reg_frames);
                        when others => rdata_i <= (others => '0');
                    end case;
                else
                    arready_i <= '0';
                    if rvalid_i = '1' and s_axi_rready = '1' then
                        rvalid_i <= '0';
                    end if;
                end if;
            end if;
        end if;
    end process;
end architecture rtl;