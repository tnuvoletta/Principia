﻿
#include "glog/logging.h"

#include <limits>
#include <tuple>
#include <utility>

#include "numerics/elliptic_integrals.hpp"
#include "numerics/polynomial.hpp"
#include "numerics/polynomial_evaluators.hpp"
#include "quantities/elementary_functions.hpp"
#include "quantities/numbers.hpp"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"

// TODO(phl):
// 1. Use arrays for the coefficients.
// 2. Use Estrin evaluation for polynomials of high degree (possibly adding
//    support for polynomials of two and three variables).
// 3. Figure something for the uninitialized variables.

// Bibliography:
// [Buli69] Bulirsch (1969), Numerical Calculation of Elliptic Integrals and
// Elliptic Fuctions.  III.
// [Fuku11a] Fukushima (2011), Precise and fast computation of the general
// complete elliptic integral of the second kind.
// [Fuku11b] Fukushima (2011), Precise and fast computation of a general
// incomplete elliptic integral of second kind by half and double argument
// transformations.
// [Fuku11c] Fukushima (2011), Precise and fast computation of a general
// incomplete elliptic integral of third kind by half and double argument
// transformations.
// [NIST10] Olver, Lozier, Boisvert, Clark Eds. (2010), NIST Handbook of
// Mathematical Functions.

namespace principia {

using quantities::Abs;
using quantities::Angle;
using quantities::Cos;
using quantities::Sin;
using quantities::Sqrt;
using quantities::si::Radian;

namespace numerics {

namespace {

// Bulirsch's cel function, [Buli69], [NIST10], 19.2(iii).
double BulirschCel(double kc, double nc, double a, double b);

// Jacobi's nome approximated by a series of the given degree.
template<int degree>
double EllipticNomeQ(double mc);

// Fukushima's complete elliptic integrals of the second kind [Fuku11a].
void FukushimaEllipticBD(double mc, double& elb, double& eld);

// Fukushima's complete elliptic integrals of the second and third kind
// [Fuku11a], [Fuku11c].
void FukushimaEllipticBDJ(double nc,
                          double mc,
                          double& bc,
                          double& dc,
                          double& jc);

// Fukushima's incomplete integrals of the second and third kind, arccos
// argument [Fuku11b], [Fuku11c].
void FukushimaEllipticBcDcJc(double c0,
                             double n,
                             double mc,
                             double& b,
                             double& d,
                             double& j);

// Fukushima's incomplete integrals of the second and third kind, arcsin
// argument [Fuku11b], [Fuku11c].
void FukushimaEllipticBsDsJs(double s0,
                             double n,
                             double mc,
                             double& b,
                             double& d,
                             double& j);

// Maclaurin series expansion of Bs and Ds [Fuku11a].
// NOTE(phl): I believe that this is a Maclaurin series but it's not completely
// clear.
void FukushimaEllipticBsDsMaclaurinSeries(double y,
                                          double m,
                                          double& b,
                                          double& d);

// Maclaurin series expansion of Js [Fuku11c].
double FukushimaEllipticJsMaclaurinSeries(double y, double n, double m);

// Fukushima's T function [Fuku11c].
double FukushimaT(double t, double h);

// A generator for the Maclaurin series for q(m) / m where q is Jacobi's nome
// function.
template<int n, template<typename, typename, int> class Evaluator>
class EllipticNomeQMaclaurin {
  static auto constexpr full_series = std::make_tuple(
    1.0 / 16.0,
    1.0 / 32.0,
    21.0 / 1024.0,
    31.0 / 2048.0,
    6257.0 / 524288.0,
    10293.0 / 1048576.0,
    279025.0 / 33554432.0,
    483127.0 / 67108864.0,
    435506703.0 / 68719476736.0,
    776957575.0 / 137438953472.0,
    22417045555.0 / 4398046511104.0,
    40784671953.0 / 8796093022208.0,
    9569130097211.0 / 2251799813685248.0,
    17652604545791.0 / 4503599627370496.0,
    523910972020563.0 / 144115188075855872.0,
    976501268709949.0 / 288230376151711744.0);

  template<typename>
  struct Generator;

  template<int... k>
  struct Generator<std::index_sequence<k...>> {
    static auto constexpr series = std::make_tuple(std::get<k>(full_series)...);
  };

 public:
  static inline PolynomialInMonomialBasis<double, double, n, Evaluator> const
      polynomial{Generator<std::make_index_sequence<n + 1>>::series};
};

// A generator for the Maclaurin series for Fukushima's T function.
template<int n, template<typename, typename, int> class Evaluator>
class FukushimaTMaclaurin {
  template<typename>
  struct Generator;

  template<int... k>
  struct Generator<std::index_sequence<k...>> {
    static auto constexpr series = std::make_tuple(1.0 / (2.0 * k + 1.0)...);
  };

 public:
  static inline PolynomialInMonomialBasis<double, double, n, Evaluator> const
      polynomial{Generator<std::make_index_sequence<n + 1>>::series};
};

using FukushimaTMaclaurin1 = FukushimaTMaclaurin<1, HornerEvaluator>;
using FukushimaTMaclaurin2 = FukushimaTMaclaurin<2, HornerEvaluator>;
using FukushimaTMaclaurin3 = FukushimaTMaclaurin<3, HornerEvaluator>;
using FukushimaTMaclaurin4 = FukushimaTMaclaurin<4, EstrinEvaluator>;
using FukushimaTMaclaurin5 = FukushimaTMaclaurin<5, EstrinEvaluator>;
using FukushimaTMaclaurin6 = FukushimaTMaclaurin<6, EstrinEvaluator>;
using FukushimaTMaclaurin7 = FukushimaTMaclaurin<7, EstrinEvaluator>;
using FukushimaTMaclaurin8 = FukushimaTMaclaurin<8, EstrinEvaluator>;
using FukushimaTMaclaurin9 = FukushimaTMaclaurin<9, EstrinEvaluator>;
using FukushimaTMaclaurin10 = FukushimaTMaclaurin<10, EstrinEvaluator>;
using FukushimaTMaclaurin11 = FukushimaTMaclaurin<11, EstrinEvaluator>;
using FukushimaTMaclaurin12 = FukushimaTMaclaurin<12, EstrinEvaluator>;

//  Double precision general complete elliptic integral "cel"
//
//  created by Burlisch
//
//  coded by T. Fukushima <Toshio.Fukushima@nao.ac.jp>
//
//  Reference: Bulirsch, R. (1969), Numer. Math., 13, 305-315
//        "Numerical computation of elliptic integrals and elliptic functions
//        III"
//
//     Inputs: kc  = complementary modulus         0 <= kc <= 1
//             nc   = complementary characteristic 0 <= nc <= 1
//             a, b = coefficients
//
//     Outputs: integral value
//
double BulirschCel(double kc,
                   double const nc,
                   double a,
                   double b) {
  // These values should give us 14 digits of accuracy, see [Buli69].
  constexpr double ca = 1.0e-7;
  constexpr double kc_nearly_0 = 1.0e-14;

  // The identifiers below follow exactly [Buli69].  Note the (uncommon) use of
  // non-const parameters to mimic [Buli69].
  double p = nc;
  if (kc == 0.0) {
    if (b == 0.0) {
      kc = kc_nearly_0;
    } else {
      // "If in this case b ≠ 0 then cel is undefined."
      DLOG(ERROR) << "kc = " << kc << " nc = " << nc << " a = " << a
                  << " b = " << b;
      return std::numeric_limits<double>::quiet_NaN();
    }
  }
  kc = Abs(kc);
  double e = kc;
  double m = 1.0;

  // Initial values for p, a, b.
  if (p > 0.0) {
    p = Sqrt(p);
    b = b / p;
  } else {
    double f = kc * kc;
    double q = 1.0 - f;
    double g = 1.0 - p;
    f = f - p;
    q = (b - a * p) * q;
    p = Sqrt(f / g);
    a = (a - b) / g;
    b = a * p - q / (g * g * p);
  }

  // Bartky's algorithm.
  for (;;) {
    double f = a;
    a = b / p + a;
    double g = e / p;
    b = f * g + b;
    b = b + b;
    p = g + p;
    g = m;
    m = kc + m;
    if (Abs(g - kc) <= g * ca) {
      break;
    }
    kc = sqrt(e);
    kc = kc + kc;
    e = kc * m;
  }
  return (π / 2) * (a * m + b) / (m * (m + p));
}

template<int degree>
double EllipticNomeQ(double const mc) {
  return mc * 
         EllipticNomeQMaclaurin<degree - 1, EstrinEvaluator>::polynomial.
             Evaluate(mc);
}

//  Double precision general complete elliptic integrals of the second kind
//
//  Reference: T. Fukushima, (2011) Math. Comp., 80, 1725-1743
//     "Precise and Fast Computation of General Complete Elliptic Integral
//      of Second Kind"
//
//     Author: T. Fukushima Toshio.Fukushima@nao.ac.jp
//
//     Inputs: mc   = complementary parameter 0 <= mc   <= 1
//
//     Output: elb,eld
//
void FukushimaEllipticBD(double const mc, double& elb, double& eld) {
  double elk, m, mx, kkc, nome;

  constexpr double K1 = 1.0 / 4.0;
  constexpr double K2 = 9.0 / 64.0;
  constexpr double K3 = 25.0 / 256.0;
  constexpr double K4 = 1225.0 / 16384.0;
  constexpr double K5 = 3969.0 / 65536.0;
  constexpr double K6 = 53361.0 / 1048576.0;
  constexpr double K7 = 184041.0 / 4194304.0;

  constexpr double B1 = 1.0 / 2.0;
  constexpr double B2 = 1.0 / 16.0;
  constexpr double B3 = 3.0 / 128.0;
  constexpr double B4 = 25.0 / 2048.0;
  constexpr double B5 = 245.0 / 32768.0;
  constexpr double B6 = 1323.0 / 262144.0;
  constexpr double B7 = 7623.0 / 2097152.0;
  constexpr double B8 = 184041.0 / 67108864.0;

  constexpr double D1 = 1.0 / 2.0;
  constexpr double D2 = 3.0 / 16.0;
  constexpr double D3 = 15.0 / 128.0;
  constexpr double D4 = 175.0 / 2048.0;
  constexpr double D5 = 2205.0 / 32768.0;
  constexpr double D6 = 14553.0 / 262144.0;
  constexpr double D7 = 99099.0 / 2097152.0;
  constexpr double D8 = 2760615.0 / 67108864.0;

  double logq2, dkkc, dddc, dele, delb, elk1;

  m = 1.0 - mc;
  if (m < 1.11e-16) {
    elb = π / 4;
    eld = π / 4;
  } else if (mc < 1.11e-16) {
    elb = 1.0;
    eld = 0.3862943611198906188344642429164 - 0.5 * log(mc);
  } else if (mc < 0.1) {
    nome = EllipticNomeQ<16>(mc);
    if (mc < 0.01) {
      dkkc = mc * (K1 +
           mc * (K2 + mc * (K3 + mc * (K4 + mc * (K5 + mc * (K6 + mc * K7))))));
      dddc = mc * (D1 +
           mc * (D2 + mc * (D3 + mc * (D4 + mc * (D5 + mc * (D6 + mc * D7))))));
    } else {
      mx = mc - 0.05;

  // (K'-1)/(π / 2)
      dkkc = 0.01286425658832983978282698630501405107893
          + mx * (0.26483429894479586582278131697637750604652
          + mx * (0.15647573786069663900214275050014481397750
          + mx * (0.11426146079748350067910196981167739749361
          + mx * (0.09202724415743445309239690377424239940545
          + mx * (0.07843218831801764082998285878311322932444
          + mx * (0.06935260142642158347117402021639363379689
          + mx * (0.06293203529021269706312943517695310879457
          + mx * (0.05821227592779397036582491084172892108196
          + mx * (0.05464909112091564816652510649708377642504
          + mx * (0.05191068843704411873477650167894906357568
          + mx * (0.04978344771840508342564702588639140680363
          + mx * (0.04812375496807025605361215168677991360500))))))))))));

  // (K'-E')/(π / 2)
      dddc = 0.02548395442966088473597712420249483947953
          + mx * (0.51967384324140471318255255900132590084179
          + mx * (0.20644951110163173131719312525729037023377
          + mx * (0.13610952125712137420240739057403788152260
          + mx * (0.10458014040566978574883406877392984277718
          + mx * (0.08674612915759188982465635633597382093113
          + mx * (0.07536380269617058326770965489534014190391
          + mx * (0.06754544594618781950496091910264174396541
          + mx * (0.06190939688096410201497509102047998554900
          + mx * (0.05771071515451786553160533778648705873199
          + mx * (0.05451217098672207169493767625617704078257
          + mx * (0.05204028407582600387265992107877094920787
          + mx * (0.05011532514520838441892567405879742720039))))))))))));
    }
    kkc = 1.0 + dkkc;
    logq2 = -0.5 * log(nome);
    elk = kkc * logq2;
    dele = -dkkc / kkc + logq2 * dddc;
    elk1 = elk - 1.0;
    delb = (dele - mc * elk1) / m;
    elb = 1.0 + delb;
    eld = elk1 - delb;
  } else if (m <= 0.01) {
    elb = (π / 2) * (B1 + m * (B2 + m * (B3 + m * (B4 + m * (B5 + m * (B6 + m *
                    (B7 + m * B8)))))));
    eld = (π / 2) * (D1 + m * (D2 + m * (D3 + m * (D4 + m * (D5 + m * (D6 + m *
                    (D7 + m * D8)))))));
  } else if (m <= 0.1) {
    mx = 0.95 - mc;
    elb = 0.790401413584395132310045630540381158921005
        + mx * (0.102006266220019154892513446364386528537788
        + mx * (0.039878395558551460860377468871167215878458
        + mx * (0.021737136375982167333478696987134316809322
        + mx * (0.013960979767622057852185340153691548520857
        + mx * (0.009892518822669142478846083436285145400444
        + mx * (0.007484612400663335676130416571517444936951
        + mx * (0.005934625664295473695080715589652011420808
        + mx * (0.004874249053581664096949448689997843978535
        + mx * (0.004114606930310886136960940893002069423559
        + mx * (0.003550452989196176932747744728766021440856
        + mx * (0.003119229959988474753291950759202798352266)))))))))));
    eld = 0.800602040206397047799296975176819811774784
        + mx * (0.313994477771767756849615832867393028789057
        + mx * (0.205913118705551954501930953451976374435088
        + mx * (0.157744346538923994475225014971416837073598
        + mx * (0.130595077319933091909091103101366509387938
        + mx * (0.113308474489758568672985167742047066367053
        + mx * (0.101454199173630195376251916342483192174927
        + mx * (0.0929187842072974367037702927967784464949434
        + mx * (0.0865653801481680871714054745336652101162894
        + mx * (0.0817279846651030135350056216958053404884715
        + mx * (0.0779906657291070378163237851392095284454654
        + mx * (0.075080426851268007156477347905308063808848)))))))))));
  } else if (m <= 0.2) {
    mx = 0.85 - mc;
    elb = 0.80102406445284489393880821604009991524037
        + mx * (0.11069534452963401497502459778015097487115
        + mx * (0.047348746716993717753569559936346358937777
        + mx * (0.028484367255041422845322166419447281776162
        + mx * (0.020277811444003597057721308432225505126013
        + mx * (0.015965005853099119442287313909177068173564
        + mx * (0.013441320273553634762716845175446390822633
        + mx * (0.011871565736951439501853534319081030547931
        + mx * (0.010868363672485520630005005782151743785248
        + mx * (0.010231587232710564565903812652581252337697
        + mx * (0.009849585546666211201566987057592610884309
        + mx * (0.009656606347153765129943681090056980586986)))))))))));
    eld = 0.834232667811735098431315595374145207701720
        + mx * (0.360495281619098275577215529302260739976126
        + mx * (0.262379664114505869328637749459234348287432
        + mx * (0.223723944518094276386520735054801578584350
        + mx * (0.206447811775681052682922746753795148394463
        + mx * (0.199809440876486856438050774316751253389944
        + mx * (0.199667451603795274869211409350873244844882
        + mx * (0.204157558868236842039815028663379643303565
        + mx * (0.212387467960572375038025392458549025660994
        + mx * (0.223948914061499360356873401571821627069173
        + mx * (0.238708097425597860161720875806632864507536
        + mx * (0.256707203545463755643710021815937785120030)))))))))));
  } else if (m <= 0.3) {
    mx = 0.75 - mc;
    elb = 0.81259777291992049322557009456643357559904
        + mx * (0.12110961794551011284012693733241967660542
        + mx * (0.057293376831239877456538980381277010644332
        + mx * (0.038509451602167328057004166642521093142114
        + mx * (0.030783430301775232744816612250699163538318
        + mx * (0.027290564934732526869467118496664914274956
        + mx * (0.025916369289445198731886546557337255438083
        + mx * (0.025847203343361799141092472018796130324244
        + mx * (0.026740923539348854616932735567182946385269
        + mx * (0.028464314554825704963640157657034405579849
        + mx * (0.030995446237278954096189768338119395563447
        + mx * (0.034384369179940975864103666824736551261799
        + mx * (0.038738002072493935952384233588242422046537))))))))))));
    eld = 0.873152581892675549645633563232643413901757
        + mx * (0.420622230667770215976919792378536040460605
        + mx * (0.344231061559450379368201151870166692934830
        + mx * (0.331133021818721761888662390999045979071436
        + mx * (0.345277285052808411877017306497954757532251
        + mx * (0.377945322150393391759797943135325823338761
        + mx * (0.427378012464553880508348757311170776829930
        + mx * (0.494671744307822405584118022550673740404732
        + mx * (0.582685115665646200824237214098764913658889
        + mx * (0.695799207728083164790111837174250683834359
        + mx * (0.840018401472533403272555302636558338772258
        + mx * (1.023268503573606060588689738498395211300483
        + mx * (1.255859085136282496149035687741403985044122))))))))))));
  } else if (m <= 0.4) {
    mx = 0.65 - mc;
    elb = 0.8253235579835158949845697805395190063745
        + mx * (0.1338621160836877898575391383950840569989
        + mx * (0.0710112935979886745743770664203746758134
        + mx * (0.0541784774173873762208472152701393154906
        + mx * (0.0494517449481029932714386586401273353617
        + mx * (0.0502221962241074764652127892365024315554
        + mx * (0.0547429131718303528104722303305931350375
        + mx * (0.0627462579270016992000788492778894700075
        + mx * (0.0746698810434768864678760362745179321956
        + mx * (0.0914808451777334717996463421986810092918
        + mx * (0.1147050921109978235104185800057554574708
        + mx * (0.1465711325814398757043492181099197917984
        + mx * (0.1902571373338462844225085057953823854177))))))))))));
    eld = 0.9190270392420973478848471774160778462738
        + mx * (0.5010021592882475139767453081737767171354
        + mx * (0.4688312705664568629356644841691659415972
        + mx * (0.5177142277764000147059587510833317474467
        + mx * (0.6208433913173031070711926900889045286988
        + mx * (0.7823643937868697229213240489900179142670
        + mx * (1.0191145350761029126165253557593691585239
        + mx * (1.3593452027484960522212885423056424704073
        + mx * (1.8457173023588279422916645725184952058635
        + mx * (2.5410717031539207287662105618152273788399
        + mx * (3.5374046552080413366422791595672470037341
        + mx * (4.9692960029774259303491034652093672488707
        + mx * (7.0338228700300311264031522795337352226926
        + mx * (10.020043225034471401553194050933390974016)))))))))))));
  } else if (m <= 0.5) {
    mx = 0.55 - mc;
    elb = 0.8394795702706129706783934654948360410325
        + mx * (0.1499164403063963359478614453083470750543
        + mx * (0.0908319358194288345999005586556105610069
        + mx * (0.0803470334833417864262134081954987019902
        + mx * (0.0856384405004704542717663971835424473169
        + mx * (0.1019547259329903716766105911448528069506
        + mx * (0.1305748115336160150072309911623351523284
        + mx * (0.1761050763588499277679704537732929242811
        + mx * (0.2468351644029554468698889593583314853486
        + mx * (0.3564244768677188553323196975301769697977
        + mx * (0.5270025622301027434418321205779314762241
        + mx * (0.7943896342593047502260866957039427731776
        + mx * (1.2167625324297180206378753787253096783993))))))))))));
    eld = 0.9744043665463696730314687662723484085813
        + mx * (0.6132468053941609101234053415051402349752
        + mx * (0.6710966695021669963502789954058993004082
        + mx * (0.8707276201850861403618528872292437242726
        + mx * (1.2295422312026907609906452348037196571302
        + mx * (1.8266059675444205694817638548699906990301
        + mx * (2.8069345309977627400322167438821024032409
        + mx * (4.4187893290840281339827573139793805587268
        + mx * (7.0832360574787653249799018590860687062869
        + mx * (11.515088120557582942290563338274745712174
        + mx * (18.931511185999274639516732819605594455165
        + mx * (31.411996938204963878089048091424028309798
        + mx * (52.520729454575828537934780076768577185134
        + mx * (88.384854735065298062125622417251073520996
        + mx * (149.56637449398047835236703116483092644714
        + mx * (254.31790843104117434615624121937495622372)))))))))))))));
  } else if (m <= 0.6) {
    mx = 0.45 - mc;
    elb = 0.8554696151564199914087224774321783838373
        + mx * (0.1708960726897395844132234165994754905373
        + mx * (0.1213352290269482260207667564010437464156
        + mx * (0.1282018835749474096272901529341076494573
        + mx * (0.1646872814515275597348427294090563472179
        + mx * (0.2374189087493817423375114793658754489958
        + mx * (0.3692081047164954516884561039890508294508
        + mx * (0.6056587338479277173311618664015401963868
        + mx * (1.0337055615578127436826717513776452476106
        + mx * (1.8189884893632678849599091011718520567105
        + mx * (3.2793776512738509375806561547016925831128
        + mx * (6.0298883807175363312261449542978750456611
        + mx * (11.269796855577941715109155203721740735793
        + mx * (21.354577850382834496786315532111529462693)))))))))))));
    eld = 1.04345529511513353426326823569160142342838
        + mx * (0.77962572192850485048535711388072271372632
        + mx * (1.02974236093206758187389128668777397528702
        + mx * (1.62203722341135313022433907993860147395972
        + mx * (2.78798953118534762046989770119382209443756
        + mx * (5.04838148737206914685643655935236541332892
        + mx * (9.46327761194348429539987572314952029503864
        + mx * (18.1814899494276679043749394081463811247757
        + mx * (35.5809805911791687037085198750213045708148
        + mx * (70.6339354619144501276254906239838074917358
        + mx * (141.828580083433059297030133195739832297859
        + mx * (287.448751250132166257642182637978103762677
        + mx * (587.115384649923076181773192202238389711345
        + mx * (1207.06543522548061603657141890778290399603
        + mx * (2495.58872724866422273012188618178997342537
        + mx * (5184.69242939480644062471334944523925163600
        + mx * (10817.2133369041327524988910635205356016939))))))))))))))));
  } else if (m <= 0.7) {
    mx = 0.35 - mc;
    elb = 0.8739200618486431359820482173294324246058
        + mx * (0.1998140574823769459497418213885348159654
        + mx * (0.1727696158780152128147094051876565603862
        + mx * (0.2281069132842021671319791750725846795701
        + mx * (0.3704681411180712197627619157146806221767
        + mx * (0.6792712528848205545443855883980014994450
        + mx * (1.3480084966817573020596179874311042267679
        + mx * (2.8276709768538207038046918250872679902352
        + mx * (6.1794682501239140840906583219887062092430
        + mx * (13.935686010342811497608625663457407447757
        + mx * (32.218929281059722026322932181420383764028
        + mx * (76.006962959226101026399085304912635262362
        + mx * (182.32144908775406957609058046006949657416
        + mx * (443.51507644112648158679360783118806161062
        + mx * (1091.8547229028388292980623647414961662223
        + mx * (2715.7658664038195881056269799613407111521)))))))))))))));
    eld = 1.13367833657573316571671258513452768536080
        + mx * (1.04864317372997039116746991765351150490010
        + mx * (1.75346504119846451588826580872136305225406
        + mx * (3.52318272680338551269021618722443199230946
        + mx * (7.74947641381397458240336356601913534598302
        + mx * (17.9864500558507330560532617743406294626849
        + mx * (43.2559163462326133313977294448984936591235
        + mx * (106.681534454096017031613223924991564429656
        + mx * (268.098486573117433951562111736259672695883
        + mx * (683.624114850289804796762005964155730439745
        + mx * (1763.49708521918740723028849567007874329637
        + mx * (4592.37475383116380899419201719007475759114
        + mx * (12053.4410190488892782190764838488156555734
        + mx * (31846.6630207420816960681624497373078887317
        + mx * (84621.2213590568080177035346867495326879117
        + mx * (225956.423182907889987641304430180593010940
        + mx * (605941.517281758859958050194535269219533685
        + mx * (1.63108259953926832083633544697688841456604e6)))))))))))))))));
  } else if (m <= 0.8) {
    mx = 0.25 - mc;
    elb = 0.895902820924731621258525533131864225704
        + mx * (0.243140003766786661947749288357729051637
        + mx * (0.273081875594105531575351304277604081620
        + mx * (0.486280007533573323895498576715458103274
        + mx * (1.082747437228230914750752674136983406683
        + mx * (2.743445290986452500459431536349945437824
        + mx * (7.555817828670234627048618342026400847824
        + mx * (22.05194082493752427472777448620986154515
        + mx * (67.15640644740229407624192175802742979626
        + mx * (211.2722537881770961691291434845898538537
        + mx * (681.9037843053270682273212958093073895805
        + mx * (2246.956231592536516768812462150619631201
        + mx * (7531.483865999711792004783423815426725079
        + mx * (25608.51260130241579018675054866136922157
        + mx * (88140.74740089604971425934283371277143256
        + mx * (306564.4242098446591430938434419151070722
        + mx * (1.076036077811072193752770590363885180738e6
        + mx * (3.807218502573632648224286313875985190526e6
        + mx * (1.356638224422139551020110323739879481042e7))))))))))))))))));
    eld = 1.26061282657491161418014946566845780315983
        + mx * (1.54866563808267658056930177790599939977154
        + mx * (3.55366941187160761540650011660758187283401
        + mx * (9.90044467610439875577300608183010716301714
        + mx * (30.3205666174524719862025105898574414438275
        + mx * (98.1802586588830891484913119780870074464833
        + mx * (329.771010434557055036273670551546757245808
        + mx * (1136.65598974289039303581967838947708073239
        + mx * (3993.83433574622979757935610692842933356144
        + mx * (14242.7295865552708506232731633468180669284
        + mx * (51394.7572916887209594591528374806790960057
        + mx * (187246.702914623152141768788258141932569037
        + mx * (687653.092375389902708761221294282367947659
        + mx * (2.54238553565398227033448846432182516906624e6
        + mx * (9.45378121934749027243313241962076028066811e6
        + mx * (3.53283630179709170835024033154326126569613e7
        + mx * (1.32593262383393014923560730485845833322771e8
        + mx * (4.99544968184054821463279808395426941549833e8
        + mx * (1.88840934729443872364972817525484292678543e9
        + mx * (7.16026753447893719179055010636502508063102e9
        + mx *
          (2.72233079469633962247554894093591262281929e10))))))))))))))))))));
  } else if (m <= 0.85) {
    mx = 0.175 - mc;
    elb = 0.915922052601931494319853880201442948834592
        + mx * (0.294714252429483394379515488141632749820347
        + mx * (0.435776709264636140422971598963772380161131
        + mx * (1.067328246493644238508159085364429570207744
        + mx * (3.327844118563268085074646976514979307993733
        + mx * (11.90406004445092906188837729711173326621810
        + mx * (46.47838820224626393512400481776284680677096
        + mx * (192.7556002578809476962739389101964074608802
        + mx * (835.3356299261900063712302517586717381557137
        + mx * (3743.124548343029102644419963712353854902019
        + mx * (17219.07731004063094108708549153310467326395
        + mx * (80904.60401669850158353080543152212152282878
        + mx * (386808.3292751742460123683674607895217760313
        + mx * (1.876487670110449342170327796786290400635732e6
        + mx * (9.216559908641567755240142886998737950775910e6))))))))))))));
    eld = 1.402200569110579095046054435635136986038164
        + mx * (2.322205897861749446534352741005347103992773
        + mx * (7.462158366466719682730245467372788273333992
        + mx * (29.43506890797307903104978364254987042421285
        + mx * (128.1590924337895775262509354898066132182429
        + mx * (591.0807036911982326384997979640812493154316
        + mx * (2830.546229607726377048576057043685514661188
        + mx * (13917.76431889392229954434840686741305556862
        + mx * (69786.10525163921228258055074102587429394212
        + mx * (355234.1420341879634781808899208309503519936
        + mx * (1.830019186413931053503912913904321703777885e6
        + mx * (9.519610812032515607466102200648641326190483e6
        + mx * (4.992086875574849453986274042758566713803723e7
        + mx * (2.635677009826023473846461512029006874800883e8
        + mx * (1.399645765120061118824228996253541612110338e9
        + mx * (7.469935792837635004663183580452618726280406e9
        + mx * (4.004155595835610574316003488168804738481448e10
        + mx *
          (2.154630668144966654449602981243932210695662e11)))))))))))))))));
  } else {
    mx = 0.125 - mc;
    elb = 0.931906061029524827613331428871579482766771
        + mx * (0.348448029538453860999386797137074571589376
        + mx * (0.666809178846938247558793864839434184202736
        + mx * (2.210769135708128662563678717558631573758222
        + mx * (9.491765048913406881414290930355300611703187
        + mx * (47.09304791027740853381457907791343619298913
        + mx * (255.9200460211233087050940506395442544885608
        + mx * (1480.029532675805407554800779436693505109703
        + mx * (8954.040904734313578374783155553041065984547
        + mx * (56052.48220982686949967604699243627330816542
        + mx * (360395.7241626000916973524840479780937869149
        + mx * (2.367539415273216077520928806581689330885103e6
        + mx * (1.582994957277684102454906900025484391190264e7
        + mx * (1.074158093278511100137056972128875270067228e8
        + mx * (7.380585460239595691878086073095523043390649e8
        + mx * (5.126022002555101496684687154904781856830296e9
        + mx * (3.593534065502416588712409180013118409428367e10
        + mx * (2.539881257612812212004146637239987308133582e11
        + mx *
          (1.808180007145359569674767150594344316702507e12))))))))))))))))));
    eld = 1.541690112721819084362258323861459983048179
        + mx * (3.379176214579645449453938918349243359477706
        + mx * (14.94058385670236671625328259137998668324435
        + mx * (81.91773929235074880784578753539752529822986
        + mx * (497.4900546551479866036061853049402721939835
        + mx * (3205.184010234846235275447901572262470252768
        + mx * (21457.32237355321925571253220641357074594515
        + mx * (147557.0156564174712105689758692929775004292
        + mx * (1.035045290185256525452269053775273002725343e6
        + mx * (7.371922334832212125197513363695905834126154e6
        + mx * (5.314344395142401141792228169170505958906345e7
        + mx * (3.868823475795976312985118115567305767603128e8
        + mx * (2.839458401528033778425531336599562337200510e9
        + mx * (2.098266122943898941547136470383199468548861e10
        + mx * (1.559617754017662417944194874282275405676282e11
        + mx * (1.165096220419884791236699872205721392201682e12
        + mx * (8.742012983013913804987431275193291316808766e12
        + mx * (6.584725462672366918676967847406180155459650e13
        + mx * (4.976798737062434393396993620379481464465749e14
        + mx * (3.773018634056605404718444239040628892506293e15
        + mx *
          (2.868263194837819660109735981973458220407767e16))))))))))))))))))));
  }
}

void FukushimaEllipticBDJ(double const nc,
                          double const mc,
                          double& bc,
                          double& dc,
                          double& jc) {
  FukushimaEllipticBD(mc, bc, dc);

  // See [Buli69], special examples after equation (1.2.2).
  double const kc = Sqrt(mc);
  jc = BulirschCel(kc, nc, /*a=*/0.0, /*b=*/1.0);
}

void FukushimaEllipticBcDcJc(double const c₀,
                             double const n,
                             double const mc,
                             double& b,
                             double& d,
                             double& j) {
  // See [Fuku11b] section 2.2 for the determination of xS.
  constexpr double xS = 0.1;
  // The maximum number of iterations in the first loop below.
  // NOTE(phl): I couldn't find a justification for this number.
  constexpr int max_transformations = 10;

  double y[max_transformations + 1];
  double s[max_transformations + 1];
  double cd[max_transformations + 1];

  double const m = 1.0 - mc;
  double const h = n * (1.0 - n) * (n - m);
  double const x₀ = c₀ * c₀;
  double const y₀ = 1.0 - x₀;

  // Alternate half and double argument transformations, when cancellations
  // would occur, [Fuku11c] section 3.3.

  // Half argument transformation of c.
  y[0] = y₀;
  s[0] = Sqrt(y₀);
  double cᵢ = c₀;
  double xᵢ = x₀;
  int i = 0;  // Note that this variable is used after the loop.
  for (; xᵢ <= xS; ++i) {
    DCHECK_LT(i, max_transformations)
        << "c₀ = " << c₀ << " n = " << n << " mc = " << mc;
    double const dᵢ = Sqrt(mc + m * xᵢ);
    xᵢ = (cᵢ + dᵢ) / (1.0 + dᵢ);
    double const yᵢ = 1.0 - xᵢ;
    y[i + 1] = yᵢ;
    s[i + 1] = Sqrt(yᵢ);
    cd[i] = cᵢ * dᵢ;
    cᵢ = Sqrt(xᵢ);
  }

  // Switch to the normal algorithm.
  FukushimaEllipticBsDsJs(s[i], n, mc, b, d, j);

  // Double argument transformation of B, D, J.
  for (int k = i; k > 0; --k) {
    double const sy = s[k - 1] * y[k];
    double const t = sy / (1.0 - n * (y[k - 1] - y[k] * cd[k - 1]));
    b = 2.0 * b - sy;
    d = d + (d + sy);
    j = j + (j + FukushimaT(t, h));
  }
}

void FukushimaEllipticBsDsJs(double const s₀,
                             double const n,
                             double const mc,
                             double& b,
                             double& d,
                             double& j) {
  // See [Fuku11c] section 3.5 for the determination of yB.
  constexpr double yB = 0.01622;
  // The maximum number of argument transformations, related to yB.  This is the
  // maximum number of iterations in the first loop below.
  constexpr int max_transformations = 10;

  double y[max_transformations + 1];
  double s[max_transformations + 1];
  double cd[max_transformations + 1];

  // Half and double argument transformations, [Fuku11c] section 3.3.
  double const m = 1.0 - mc;
  double const h = n * (1.0 - n) * (n - m);
  double const y₀ = s₀ * s₀;

  // Half argument transformation of s.
  y[0] = y₀;
  s[0] = s₀;
  double yᵢ = y₀;
  int i = 0;  // Note that this variable is used after the loop.
  for (; yᵢ >= yB; ++i) {
    DCHECK_LT(i, max_transformations)
        << "s₀ = " << s₀ << " n = " << n << " mc = " << mc;
    double const cᵢ = Sqrt(1.0 - yᵢ);
    double const dᵢ = Sqrt(1.0 - m * yᵢ);
    yᵢ = yᵢ / ((1.0 + cᵢ) * (1.0 + dᵢ));
    y[i + 1] = yᵢ;
    s[i + 1] = Sqrt(yᵢ);
    cd[i] = cᵢ * dᵢ;
  }

  // Maclaurin series.
  FukushimaEllipticBsDsMaclaurinSeries(yᵢ, m, b, d);
  b = s[i] * b;
  d = s[i] * yᵢ * d;
  j = s[i] * FukushimaEllipticJsMaclaurinSeries(yᵢ, n, m);

  // Double argument transformation of B, D, J.
  for (int k = i; k > 0; --k) {
    double const sy = s[k - 1] * y[k];
    double const t = sy / (1.0 - n * (y[k - 1] - y[k] * cd[k - 1]));
    b = 2.0 * b - sy;
    d = d + (d + sy);
    j = j + (j + FukushimaT(t, h));
  }
}

void FukushimaEllipticBsDsMaclaurinSeries(double const y,
                                          double const m,
                                          double& b,
                                          double& d) {
  constexpr double F10 = 1.0 / 6.0;
  constexpr double F20 = 3.0 / 40.0;
  constexpr double F21 = 2.0 / 40.0;
  constexpr double F30 = 5.0 / 112.0;
  constexpr double F31 = 3.0 / 112.0;
  constexpr double F40 = 35.0 / 1152.0;
  constexpr double F41 = 20.0 / 1152.0;
  constexpr double F42 = 18.0 / 1152.0;
  constexpr double F50 = 63.0 / 2816.0;
  constexpr double F51 = 35.0 / 2816.0;
  constexpr double F52 = 30.0 / 2816.0;
  constexpr double F60 = 231.0 / 13312.0;
  constexpr double F61 = 126.0 / 13312.0;
  constexpr double F62 = 105.0 / 13312.0;
  constexpr double F63 = 100.0 / 13312.0;
  constexpr double F70 = 429.0 / 30720.0;
  constexpr double F71 = 231.0 / 30720.0;
  constexpr double F72 = 189.0 / 30720.0;
  constexpr double F73 = 175.0 / 30720.0;
  constexpr double F80 = 6435.0 / 557056.0;
  constexpr double F81 = 3432.0 / 557056.0;
  constexpr double F82 = 2722.0 / 557056.0;
  constexpr double F83 = 2520.0 / 557056.0;
  constexpr double F84 = 2450.0 / 557056.0;
  constexpr double F90 = 12155.0 / 1245184.0;
  constexpr double F91 = 6435.0 / 1245184.0;
  constexpr double F92 = 5148.0 / 1245184.0;
  constexpr double F93 = 4620.0 / 1245184.0;
  constexpr double F94 = 4410.0 / 1245184.0;
  constexpr double FA0 = 46189.0 / 5505024.0;
  constexpr double FA1 = 24310.0 / 5505024.0;
  constexpr double FA2 = 19305.0 / 5505024.0;
  constexpr double FA3 = 17160.0 / 5505024.0;
  constexpr double FA4 = 16170.0 / 5505024.0;
  constexpr double FA5 = 15876.0 / 5505024.0;
  constexpr double FB0 = 88179.0 / 12058624.0;
  constexpr double FB1 = 46189.0 / 12058624.0;
  constexpr double FB2 = 36465.0 / 12058624.0;
  constexpr double FB3 = 32175.0 / 12058624.0;
  constexpr double FB4 = 30030.0 / 12058624.0;
  constexpr double FB5 = 29106.0 / 12058624.0;

  constexpr double A1 = 3.0 / 5.0;
  constexpr double A2 = 5.0 / 7.0;
  constexpr double A3 = 7.0 / 9.0;
  constexpr double A4 = 9.0 / 11.0;
  constexpr double A5 = 11.0 / 13.0;
  constexpr double A6 = 13.0 / 15.0;
  constexpr double A7 = 15.0 / 17.0;
  constexpr double A8 = 17.0 / 19.0;
  constexpr double A9 = 19.0 / 21.0;
  constexpr double AA = 21.0 / 23.0;
  constexpr double AB = 23.0 / 25.0;

  double B1, B2, B3, B4, B5, B6, B7, B8, B9, BA, BB;
  double D1, D2, D3, D4, D5, D6, D7, D8, D9, DA, DB;
  constexpr double D0 = 1.0 / 3.0;

  double F1, F2, F3, F4, F5, F6, F7, F8, F9, FA, FB;
  F1 = F10 + m * F10;
  F2 = F20 + m * (F21 + m * F20);
  F3 = F30 + m * (F31 + m * (F31 + m * F30));
  F4 = F40 + m * (F41 + m * (F42 + m * (F41 + m * F40)));
  F5 = F50 + m * (F51 + m * (F52 + m * (F52 + m * (F51 + m * F50))));
  F6 = F60 + m * (F61 + m * (F62 + m * (F63 + m * (F62 + m * (F61 +
       m * F60)))));
  F7 = F70 + m * (F71 + m * (F72 + m * (F73 + m * (F73 + m * (F72 + m * (F71 +
       m * F70))))));
  F8 = F80 + m * (F81 + m * (F82 + m * (F83 + m * (F84 + m * (F83 + m * (F82 +
       m * (F81 + m * F80)))))));
  F9 = F90 + m * (F91 + m * (F92 + m * (F93 + m * (F94 + m * (F94 + m * (F93 +
       m * (F92 + m * (F91 + m * F90))))))));
  FA = FA0 + m * (FA1 + m * (FA2 + m * (FA3 + m * (FA4 + m * (FA5 + m * (FA4 +
       m * (FA3 + m * (FA2 + m * (FA1 + m * FA0)))))))));
  FB = FB0 + m * (FB1 + m * (FB2 + m * (FB3 + m * (FB4 + m * (FB5 + m * (FB5 +
       m * (FB4 + m * (FB3 + m * (FB2 + m * (FB1 + m * FB0))))))))));

  D1 = F1 * A1;
  D2 = F2 * A2;
  D3 = F3 * A3;
  D4 = F4 * A4;
  D5 = F5 * A5;
  D6 = F6 * A6;
  D7 = F7 * A7;
  D8 = F8 * A8;
  D9 = F9 * A9;
  DA = FA * AA;
  DB = FB * AB;

  d = D0 +
      y * (D1 +
           y * (D2 +
                y * (D3 +
                     y * (D4 +
                          y * (D5 +
                               y * (D6 +
                                    y * (D7 +
                                         y * (D8 +
                                              y * (D9 +
                                                   y * (DA + y * DB))))))))));

  B1 = F1 - D0;
  B2 = F2 - D1;
  B3 = F3 - D2;
  B4 = F4 - D3;
  B5 = F5 - D4;
  B6 = F6 - D5;
  B7 = F7 - D6;
  B8 = F8 - D7;
  B9 = F9 - D8;
  BA = FA - D9;
  BB = FB - DA;

  b = 1.0 +
      y * (B1 +
           y * (B2 +
                y * (B3 +
                     y * (B4 +
                          y * (B5 +
                               y * (B6 +
                                    y * (B7 +
                                         y * (B8 +
                                              y * (B9 +
                                                   y * (BA + y * BB))))))))));
}

double FukushimaEllipticJsMaclaurinSeries(double const y,
                                          double const n,
                                          double const m) {
  constexpr double J100 = 1.0 / 3.0;

  constexpr double J200 = 1.0 / 10.0;
  constexpr double J201 = 2.0 / 10.0;
  constexpr double J210 = 1.0 / 10.0;

  constexpr double J300 = 3.0 / 56.0;
  constexpr double J301 = 4.0 / 56.0;
  constexpr double J302 = 8.0 / 56.0;
  constexpr double J310 = 2.0 / 56.0;
  constexpr double J311 = 4.0 / 56.0;
  constexpr double J320 = 3.0 / 56.0;

  constexpr double J400 = 5.0 / 144.0;
  constexpr double J401 = 6.0 / 144.0;
  constexpr double J402 = 8.0 / 144.0;
  constexpr double J403 = 16.0 / 144.0;
  constexpr double J410 = 3.0 / 144.0;
  constexpr double J411 = 4.0 / 144.0;
  constexpr double J412 = 8.0 / 144.0;
  constexpr double J420 = 3.0 / 144.0;
  constexpr double J421 = 6.0 / 144.0;
  constexpr double J430 = 5.0 / 144.0;

  constexpr double J500 = 35.0 / 1408.0;
  constexpr double J501 = 40.0 / 1408.0;
  constexpr double J502 = 48.0 / 1408.0;
  constexpr double J503 = 64.0 / 1408.0;
  constexpr double J504 = 128.0 / 1408.0;
  constexpr double J510 = 20.0 / 1408.0;
  constexpr double J511 = 24.0 / 1408.0;
  constexpr double J512 = 32.0 / 1408.0;
  constexpr double J513 = 64.0 / 1408.0;
  constexpr double J520 = 18.0 / 1408.0;
  constexpr double J521 = 24.0 / 1408.0;
  constexpr double J522 = 48.0 / 1408.0;
  constexpr double J530 = 20.0 / 1408.0;
  constexpr double J531 = 40.0 / 1408.0;
  constexpr double J540 = 35.0 / 1408.0;

  constexpr double J600 = 63.0 / 3328.0;
  constexpr double J601 = 70.0 / 3328.0;
  constexpr double J602 = 80.0 / 3328.0;
  constexpr double J603 = 96.0 / 3328.0;
  constexpr double J604 = 128.0 / 3328.0;
  constexpr double J605 = 256.0 / 3328.0;
  constexpr double J610 = 35.0 / 3328.0;
  constexpr double J611 = 40.0 / 3328.0;
  constexpr double J612 = 48.0 / 3328.0;
  constexpr double J613 = 64.0 / 3328.0;
  constexpr double J614 = 128.0 / 3328.0;
  constexpr double J620 = 30.0 / 3328.0;
  constexpr double J621 = 36.0 / 3328.0;
  constexpr double J622 = 48.0 / 3328.0;
  constexpr double J623 = 96.0 / 3328.0;
  constexpr double J630 = 30.0 / 3328.0;
  constexpr double J631 = 40.0 / 3328.0;
  constexpr double J632 = 80.0 / 3328.0;
  constexpr double J640 = 35.0 / 3328.0;
  constexpr double J641 = 70.0 / 3328.0;
  constexpr double J650 = 63.0 / 3328.0;

  constexpr double J700 = 231.0 / 15360.0;
  constexpr double J701 = 252.0 / 15360.0;
  constexpr double J702 = 280.0 / 15360.0;
  constexpr double J703 = 320.0 / 15360.0;
  constexpr double J704 = 384.0 / 15360.0;
  constexpr double J705 = 512.0 / 15360.0;
  constexpr double J706 = 1024.0 / 15360.0;
  constexpr double J710 = 126.0 / 15360.0;
  constexpr double J711 = 140.0 / 15360.0;
  constexpr double J712 = 160.0 / 15360.0;
  constexpr double J713 = 192.0 / 15360.0;
  constexpr double J714 = 256.0 / 15360.0;
  constexpr double J715 = 512.0 / 15360.0;
  constexpr double J720 = 105.0 / 15360.0;
  constexpr double J721 = 120.0 / 15360.0;
  constexpr double J722 = 144.0 / 15360.0;
  constexpr double J723 = 192.0 / 15360.0;
  constexpr double J724 = 384.0 / 15360.0;
  constexpr double J730 = 100.0 / 15360.0;
  constexpr double J731 = 120.0 / 15360.0;
  constexpr double J732 = 160.0 / 15360.0;
  constexpr double J733 = 320.0 / 15360.0;
  constexpr double J740 = 105.0 / 15360.0;
  constexpr double J741 = 140.0 / 15360.0;
  constexpr double J742 = 280.0 / 15360.0;
  constexpr double J750 = 126.0 / 15360.0;
  constexpr double J751 = 252.0 / 15360.0;
  constexpr double J760 = 231.0 / 15360.0;

  constexpr double J800 = 429.0 / 34816.0;
  constexpr double J801 = 462.0 / 34816.0;
  constexpr double J802 = 504.0 / 34816.0;
  constexpr double J803 = 560.0 / 34816.0;
  constexpr double J804 = 640.0 / 34816.0;
  constexpr double J805 = 768.0 / 34816.0;
  constexpr double J806 = 1024.0 / 34816.0;
  constexpr double J807 = 2048.0 / 34816.0;
  constexpr double J810 = 231.0 / 34816.0;
  constexpr double J811 = 252.0 / 34816.0;
  constexpr double J812 = 280.0 / 34816.0;
  constexpr double J813 = 320.0 / 34816.0;
  constexpr double J814 = 384.0 / 34816.0;
  constexpr double J815 = 512.0 / 34816.0;
  constexpr double J816 = 1024.0 / 34816.0;
  constexpr double J820 = 189.0 / 34816.0;
  constexpr double J821 = 210.0 / 34816.0;
  constexpr double J822 = 240.0 / 34816.0;
  constexpr double J823 = 288.0 / 34816.0;
  constexpr double J824 = 284.0 / 34816.0;
  constexpr double J825 = 768.0 / 34816.0;
  constexpr double J830 = 175.0 / 34816.0;
  constexpr double J831 = 200.0 / 34816.0;
  constexpr double J832 = 240.0 / 34816.0;
  constexpr double J833 = 320.0 / 34816.0;
  constexpr double J834 = 640.0 / 34816.0;
  constexpr double J840 = 175.0 / 34816.0;
  constexpr double J841 = 210.0 / 34816.0;
  constexpr double J842 = 280.0 / 34816.0;
  constexpr double J843 = 560.0 / 34816.0;
  constexpr double J850 = 189.0 / 34816.0;
  constexpr double J851 = 252.0 / 34816.0;
  constexpr double J852 = 504.0 / 34816.0;
  constexpr double J860 = 231.0 / 34816.0;
  constexpr double J861 = 462.0 / 34816.0;
  constexpr double J870 = 429.0 / 34816.0;

  constexpr double J900 = 6435.0 / 622592.0;
  constexpr double J901 = 6864.0 / 622592.0;
  constexpr double J902 = 7392.0 / 622592.0;
  constexpr double J903 = 8064.0 / 622592.0;
  constexpr double J904 = 8960.0 / 622592.0;
  constexpr double J905 = 10240.0 / 622592.0;
  constexpr double J906 = 12288.0 / 622592.0;
  constexpr double J907 = 16384.0 / 622592.0;
  constexpr double J908 = 32768.0 / 622592.0;
  constexpr double J910 = 3432.0 / 622592.0;
  constexpr double J911 = 3696.0 / 622592.0;
  constexpr double J912 = 4032.0 / 622592.0;
  constexpr double J913 = 4480.0 / 622592.0;
  constexpr double J914 = 5120.0 / 622592.0;
  constexpr double J915 = 6144.0 / 622592.0;
  constexpr double J916 = 8192.0 / 622592.0;
  constexpr double J917 = 16384.0 / 622592.0;
  constexpr double J920 = 2772.0 / 622592.0;
  constexpr double J921 = 3024.0 / 622592.0;
  constexpr double J922 = 3360.0 / 622592.0;
  constexpr double J923 = 3840.0 / 622592.0;
  constexpr double J924 = 4608.0 / 622592.0;
  constexpr double J925 = 6144.0 / 622592.0;
  constexpr double J926 = 12288.0 / 622592.0;
  constexpr double J930 = 2520.0 / 622592.0;
  constexpr double J931 = 2800.0 / 622592.0;
  constexpr double J932 = 3200.0 / 622592.0;
  constexpr double J933 = 3840.0 / 622592.0;
  constexpr double J934 = 5120.0 / 622592.0;
  constexpr double J935 = 10240.0 / 622592.0;
  constexpr double J940 = 2450.0 / 622592.0;
  constexpr double J941 = 2800.0 / 622592.0;
  constexpr double J942 = 3360.0 / 622592.0;
  constexpr double J943 = 4480.0 / 622592.0;
  constexpr double J944 = 8960.0 / 622592.0;
  constexpr double J950 = 2520.0 / 622592.0;
  constexpr double J951 = 3024.0 / 622592.0;
  constexpr double J952 = 4032.0 / 622592.0;
  constexpr double J953 = 8064.0 / 622592.0;
  constexpr double J960 = 2772.0 / 622592.0;
  constexpr double J961 = 3696.0 / 622592.0;
  constexpr double J962 = 7392.0 / 622592.0;
  constexpr double J970 = 3432.0 / 622592.0;
  constexpr double J971 = 6864.0 / 622592.0;
  constexpr double J980 = 6435.0 / 622592.0;

  constexpr double JA00 = 12155.0 / 1376256.0;
  constexpr double JA01 = 12870.0 / 1376256.0;
  constexpr double JA02 = 13728.0 / 1376256.0;
  constexpr double JA03 = 14784.0 / 1376256.0;
  constexpr double JA04 = 16128.0 / 1376256.0;
  constexpr double JA05 = 17920.0 / 1376256.0;
  constexpr double JA06 = 20480.0 / 1376256.0;
  constexpr double JA07 = 24576.0 / 1376256.0;
  constexpr double JA08 = 32768.0 / 1376256.0;
  constexpr double JA09 = 65536.0 / 1376256.0;
  constexpr double JA10 = 6435.0 / 1376256.0;
  constexpr double JA11 = 6864.0 / 1376256.0;
  constexpr double JA12 = 7392.0 / 1376256.0;
  constexpr double JA13 = 8064.0 / 1376256.0;
  constexpr double JA14 = 8960.0 / 1376256.0;
  constexpr double JA15 = 10240.0 / 1376256.0;
  constexpr double JA16 = 12288.0 / 1376256.0;
  constexpr double JA17 = 16384.0 / 1376256.0;
  constexpr double JA18 = 32768.0 / 1376256.0;
  constexpr double JA20 = 5148.0 / 1376256.0;
  constexpr double JA21 = 5544.0 / 1376256.0;
  constexpr double JA22 = 6048.0 / 1376256.0;
  constexpr double JA23 = 6720.0 / 1376256.0;
  constexpr double JA24 = 7680.0 / 1376256.0;
  constexpr double JA25 = 9216.0 / 1376256.0;
  constexpr double JA26 = 12288.0 / 1376256.0;
  constexpr double JA27 = 24576.0 / 1376256.0;
  constexpr double JA30 = 4620.0 / 1376256.0;
  constexpr double JA31 = 5040.0 / 1376256.0;
  constexpr double JA32 = 5600.0 / 1376256.0;
  constexpr double JA33 = 6400.0 / 1376256.0;
  constexpr double JA34 = 7680.0 / 1376256.0;
  constexpr double JA35 = 10240.0 / 1376256.0;
  constexpr double JA36 = 20480.0 / 1376256.0;
  constexpr double JA40 = 4410.0 / 1376256.0;
  constexpr double JA41 = 4900.0 / 1376256.0;
  constexpr double JA42 = 5600.0 / 1376256.0;
  constexpr double JA43 = 6720.0 / 1376256.0;
  constexpr double JA44 = 8960.0 / 1376256.0;
  constexpr double JA45 = 17920.0 / 1376256.0;
  constexpr double JA50 = 4410.0 / 1376256.0;
  constexpr double JA51 = 5040.0 / 1376256.0;
  constexpr double JA52 = 6048.0 / 1376256.0;
  constexpr double JA53 = 8064.0 / 1376256.0;
  constexpr double JA54 = 16128.0 / 1376256.0;
  constexpr double JA60 = 4620.0 / 1376256.0;
  constexpr double JA61 = 5544.0 / 1376256.0;
  constexpr double JA62 = 7392.0 / 1376256.0;
  constexpr double JA63 = 14784.0 / 1376256.0;
  constexpr double JA70 = 5148.0 / 1376256.0;
  constexpr double JA71 = 6864.0 / 1376256.0;
  constexpr double JA72 = 13728.0 / 1376256.0;
  constexpr double JA80 = 6435.0 / 1376256.0;
  constexpr double JA81 = 12870.0 / 1376256.0;
  constexpr double JA90 = 12155.0 / 1376256.0;

  double J1, J2, J3, J4, J5, J6, J7, J8, J9, JA;

  J1 = J100;
  J2 = J200 + n * J201 + m * J210;
  J3 = J300 + n * (J301 + n * J302) + m * (J310 + n * J311 + m * J320);
  J4 = J400 + n * (J401 + n * (J402 + n * J403)) +
       m * (J410 + n * (J411 + n * J412) + m * (J420 + n * J421 + m * J430));
  J5 = J500 + n * (J501 + n * (J502 + n * (J503 + n * J504))) +
       m * (J510 + n * (J511 + n * (J512 + n * J513)) +
            m * (J520 + n * (J521 + n * J522) +
                 m * (J530 + n * J531 + m * J540)));
  if (y <= 6.0369310e-04) {
    return y * (J1 + y * (J2 + y * (J3 + y * (J4 + y * J5))));
  }

  J6 = J600 + n * (J601 + n * (J602 + n * (J603 + n * (J604 + n * J605)))) +
       m * (J610 + n * (J611 + n * (J612 + n * (J613 + n * J614))) +
            m * (J620 + n * (J621 + n * (J622 + n * J623)) +
                 m * (J630 + n * (J631 + n * J632) +
                      m * (J640 + n * J641 + m * J650))));
  if (y <= 2.0727505e-03) {
    return y * (J1 + y * (J2 + y * (J3 + y * (J4 + y * (J5 + y * J6)))));
  }

  J7 =
      J700 +
      n * (J701 +
           n * (J702 + n * (J703 + n * (J704 + n * (J705 + n * J706))))) +
      m * (J710 + n * (J711 + n * (J712 + n * (J713 + n * (J714 + n * J715)))) +
           m * (J720 + n * (J721 + n * (J722 + n * (J723 + n * J724))) +
                m * (J730 + n * (J731 + n * (J732 + n * J733)) +
                     m * (J740 + n * (J741 + n * J742) +
                          m * (J750 + n * J751 + m * J760)))));
  if (y <= 5.0047026e-03) {
    return y *
           (J1 + y * (J2 + y * (J3 + y * (J4 + y * (J5 + y * (J6 + y * J7))))));
  }

  J8 =
      J800 +
      n * (J801 +
           n * (J802 +
                n * (J803 + n * (J804 + n * (J805 + n * (J806 + n * J807)))))) +
      m * (J810 +
           n * (J811 +
                n * (J812 + n * (J813 + n * (J814 + n * (J815 + n * J816))))) +
           m * (J820 +
                n * (J821 + n * (J822 + n * (J823 + n * (J824 + n * J825)))) +
                m * (J830 + n * (J831 + n * (J832 + n * (J833 + n * J834))) +
                     m * (J840 + n * (J841 + n * (J842 + n * J843)) +
                          m * (J850 + n * (J851 + n * J852) +
                               m * (J860 + n * J861 + m * J870))))));
  if (y <= 9.6961652e-03) {
    return y * (J1 +
                y * (J2 +
                     y * (J3 +
                          y * (J4 + y * (J5 + y * (J6 + y * (J7 + y * J8)))))));
  }

  J9 = J900 +
       n * (J901 +
            n * (J902 +
                 n * (J903 +
                      n * (J904 +
                           n * (J905 + n * (J906 + n * (J907 + n * J908))))))) +
       m * (J910 +
            n * (J911 +
                 n * (J912 +
                      n * (J913 +
                           n * (J914 + n * (J915 + n * (J916 + n * J917)))))) +
            m * (J920 +
                 n * (J921 +
                      n * (J922 +
                           n * (J923 + n * (J924 + n * (J925 + n * J926))))) +
                 m * (J930 +
                      n * (J931 +
                           n * (J932 + n * (J933 + n * (J934 + n * J935)))) +
                      m * (J940 +
                           n * (J941 + n * (J942 + n * (J943 + n * J944))) +
                           m * (J950 + n * (J951 + n * (J952 + n * J953)) +
                                m * (J960 + n * (J961 + n * J962) +
                                     m * (J970 + n * J971 + m * J980)))))));
  if (y <= 1.6220210e-02) {
    return y *
           (J1 +
            y * (J2 +
                 y * (J3 +
                      y * (J4 +
                           y * (J5 +
                                y * (J6 + y * (J7 + y * (J8 + y * J9))))))));
  }

  JA =
      JA00 +
      n * (JA01 +
           n * (JA02 +
                n * (JA03 +
                     n * (JA04 +
                          n * (JA05 +
                               n * (JA06 +
                                    n * (JA07 + n * (JA08 + n * JA09)))))))) +
      m * (JA10 +
           n * (JA11 +
                n * (JA12 +
                     n * (JA13 +
                          n * (JA14 +
                               n * (JA15 +
                                    n * (JA16 + n * (JA17 + n * JA18))))))) +
           m * (JA20 +
                n * (JA21 +
                     n * (JA22 +
                          n * (JA23 +
                               n * (JA24 +
                                    n * (JA25 + n * (JA26 + n * JA27)))))) +
                m * (JA30 +
                     n * (JA31 +
                          n * (JA32 +
                               n * (JA33 +
                                    n * (JA34 + n * (JA35 + n * JA36))))) +
                     m * (JA40 +
                          n * (JA41 +
                               n * (JA42 +
                                    n * (JA43 + n * (JA44 + n * JA45)))) +
                          m * (JA50 +
                               n * (JA51 + n * (JA52 + n * (JA53 + n * JA54))) +
                               m * (JA60 + n * (JA61 + n * (JA62 + n * JA63)) +
                                    m * (JA70 + n * (JA71 + n * JA72) +
                                         m * (JA80 + n * JA81 +
                                              m * JA90))))))));
  return y * (J1 +
              y * (J2 +
                   y * (J3 +
                        y * (J4 +
                             y * (J5 +
                                  y * (J6 +
                                       y * (J7 +
                                            y * (J8 + y * (J9 + y * JA)))))))));
}

double FukushimaT(double const t, double const h) {
  double const z = -h * t * t;
  double const abs_z = abs(z);

  // NOTE(phl): One might be tempted to rewrite this statement using a binary
  // split of the interval [0, 1], but according to Table 1 of [Fuku11c] the
  // distribution of z is very biased towards the small values, so this is
  // simpler and probably better.  (It also explains the position of z < 0 in
  // the list.)
  if (abs_z < 3.3306691e-16) {
    return t;
  } else if (abs_z < 2.3560805e-08) {
    return t * FukushimaTMaclaurin1::polynomial.Evaluate(z);
  } else if (abs_z < 9.1939631e-06) {
    return t * FukushimaTMaclaurin2::polynomial.Evaluate(z);
  } else if (abs_z < 1.7779240e-04) {
    return t * FukushimaTMaclaurin3::polynomial.Evaluate(z);
  } else if (abs_z < 1.0407839e-03) {
    return t * FukushimaTMaclaurin4::polynomial.Evaluate(z);
  } else if (abs_z < 3.3616998e-03) {
    return t * FukushimaTMaclaurin5::polynomial.Evaluate(z);
  } else if (abs_z < 7.7408014e-03) {
    return t * FukushimaTMaclaurin6::polynomial.Evaluate(z);
  } else if (abs_z < 1.4437181e-02) {
    return t * FukushimaTMaclaurin7::polynomial.Evaluate(z);
  } else if (abs_z < 2.3407312e-02) {
    return t * FukushimaTMaclaurin8::polynomial.Evaluate(z);
  } else if (abs_z < 3.4416203e-02) {
    return t * FukushimaTMaclaurin9::polynomial.Evaluate(z);
  } else if (z < 0.0) {
    double const r = Sqrt(h);
    double const ri = 1.0 / r;
    return std::atan(r * t) / r;
  } else if (abs_z < 4.7138547e-02) {
    return t * FukushimaTMaclaurin10::polynomial.Evaluate(z);
  } else if (abs_z < 6.1227405e-02) {
    return t * FukushimaTMaclaurin11::polynomial.Evaluate(z);
  } else if (abs_z < 7.6353468e-02) {
    return t * FukushimaTMaclaurin12::polynomial.Evaluate(z);
  } else {
    double const r = Sqrt(-h);
    return std::atanh(r * t) / r;
  }
}

}  // namespace

// Double precision general incomplete elliptic integrals of all three kinds
//
//     Reference: T. Fukushima, (2011) J. Comp. Appl. Math., 236, 1961-1975
//     "Precise and Fast Computation of a General Incomplete Elliptic Integral
//     of Third Kind by Half and Double Argument Transformations"
//
//     Author: T. Fukushima Toshio.Fukushima@nao.ac.jp
//
//     Inputs: φ  = argument                  0 <= φ  <= π / 2
//             n    = characteristic          0 <= n  <= 1
//             mc   = complementary parameter 0 <= mc <= 1
//
//     Outputs: b, d, j
//
void FukushimaEllipticBDJ(Angle const& φ,
                          double const n,
                          double const mc,
                          double& b,
                          double& d,
                          double& j) {
  // NOTE(phl): The original Fortran code had φs = 1.345 * Radian, which,
  // according to the above-mentioned paper, is suitable for single precision.
  // However, this is double precision.  Importantly, this doesn't match the
  // value of ys.  The discrepancy has a 5-10% impact on performance.  I am not
  // sure if it has an impact on correctness.

  // Sin(φs)^2 must be approximately ys.
  constexpr Angle φs = 1.249 * Radian;
  constexpr double ys = 0.9;

  // The selection rule in [Fuku11b] section 2.1, equations (7-11) and [Fuku11c]
  // section 3.2, equations (22) and (23).  The identifiers follow Fukushima's
  // notation.
  // NOTE(phl): The computation of 1 - c² loses accuracy with respect to the
  // evaluation of Sin(φ).
  if (φ < φs) {
    FukushimaEllipticBsDsJs(Sin(φ), n, mc, b, d, j);
  } else {
    double const m = 1.0 - mc;
    double const nc = 1.0 - n;
    double const h = n * nc * (n - m);
    double const c = Cos(φ);
    double const c² = c * c;
    double const z²_denominator = mc + m * c²;
    if (c² < ys * z²_denominator) {
      double bc, dc, jc;
      double const z = c / Sqrt(z²_denominator);
      FukushimaEllipticBsDsJs(z, n, mc, b, d, j);
      FukushimaEllipticBDJ(nc, mc, bc, dc, jc);
      double const sz = z * Sqrt(1.0 - c²);
      double const t = sz / nc;
      b = bc - (b - sz);
      d = dc - (d + sz);
      j = jc - (j + FukushimaT(t, h));
    } else {
      double const w²_numerator = mc * (1.0 - c²);
      if (w²_numerator < c² * z²_denominator) {
        FukushimaEllipticBcDcJc(c, n, mc, b, d, j);
      } else {
        double bc, dc, jc;
        double const w²_denominator = z²_denominator;
        double const w²_over_mc = (1.0 - c²) / w²_denominator;
        FukushimaEllipticBcDcJc(Sqrt(mc * w²_over_mc), n, mc, b, d, j);
        FukushimaEllipticBDJ(nc, mc, bc, dc, jc);
        double const sz = c * Sqrt(w²_over_mc);
        double const t = sz / nc;
        b = bc - (b - sz);
        d = dc - (d + sz);
        j = jc - (j + FukushimaT(t, h));
      }
    }
  }
}

//  Double precision complete elliptic integral of the first kind
//
//     Reference: T. Fukushima, (2009) Celest. Mech. Dyn. Astron. 105, 305-328
//        "Fast Computation of Complete Elliptic Integrlals and Jacobian
//         Elliptic Functions"
//
//     Author: T. Fukushima Toshio.Fukushima@nao.ac.jp
//
//     Inputs: mc   = complementary parameter 0 <= mc   <= 1
//
double EllipticK(double const mc) {
  double m, mx;
  double kkc, nome;

  constexpr double D1 = 1.0 / 16.0;
  constexpr double D2 = 1.0 / 32.0;
  constexpr double D3 = 21.0 / 1024.0;
  constexpr double D4 = 31.0 / 2048.0;
  constexpr double D5 = 6257.0 / 524288.0;
  constexpr double D6 = 10293.0 / 1048576.0;
  constexpr double D7 = 279025.0 / 33554432.0;
  constexpr double D8 = 483127.0 / 67108864.0;
  constexpr double D9 = 435506703.0 / 68719476736.0;
  constexpr double D10 = 776957575.0 / 137438953472.0;
  constexpr double D11 = 22417045555.0 / 4398046511104.0;
  constexpr double D12 = 40784671953.0 / 8796093022208.0;
  constexpr double D13 = 9569130097211.0 / 2251799813685248.0;
  constexpr double D14 = 17652604545791.0 / 4503599627370496.0;
  constexpr double TINY = 1.0e-99;

  m = 1.0 - mc;
  if (abs(m) < 1.0e-16) {
    return π / 2;
  } else if (mc < TINY) {
    return 1.3862943611198906 - 0.5 * log(TINY);
  } else if (mc < 1.11e-16) {
    return 1.3862943611198906 - 0.5 * log(mc);
  } else if (mc < 0.1) {
    nome = EllipticNomeQ<14>(mc);
    mx = mc - 0.05;
    //
    //  K'
    //
    kkc = 1.591003453790792180 + mx * (
         0.416000743991786912 + mx * (
         0.245791514264103415 + mx * (
         0.179481482914906162 + mx * (
         0.144556057087555150 + mx * (
         0.123200993312427711 + mx * (
         0.108938811574293531 + mx * (
         0.098853409871592910 + mx * (
         0.091439629201749751 + mx * (
         0.085842591595413900 + mx * (
         0.081541118718303215))))))))));
    return -kkc * (1 / π) * log(nome);
  } else if (m <= 0.1) {
    mx = m - 0.05;
    return 1.591003453790792180 + mx * (
         0.416000743991786912 + mx * (
         0.245791514264103415 + mx * (
         0.179481482914906162 + mx * (
         0.144556057087555150 + mx * (
         0.123200993312427711 + mx * (
         0.108938811574293531 + mx * (
         0.098853409871592910 + mx * (
         0.091439629201749751 + mx * (
         0.085842591595413900 + mx * (
         0.081541118718303215))))))))));
  } else if (m <= 0.2) {
    mx = m - 0.15;
    return 1.635256732264579992 + mx * (
         0.471190626148732291 + mx * (
         0.309728410831499587 + mx * (
         0.252208311773135699 + mx * (
         0.226725623219684650 + mx * (
         0.215774446729585976 + mx * (
         0.213108771877348910 + mx * (
         0.216029124605188282 + mx * (
         0.223255831633057896 + mx * (
         0.234180501294209925 + mx * (
         0.248557682972264071 + mx * (
         0.266363809892617521 + mx * (
         0.287728452156114668))))))))))));
  } else if (m <= 0.3) {
    mx = m - 0.25;
    return 1.685750354812596043 + mx * (
         0.541731848613280329 + mx * (
         0.401524438390690257 + mx * (
         0.369642473420889090 + mx * (
         0.376060715354583645 + mx * (
         0.405235887085125919 + mx * (
         0.453294381753999079 + mx * (
         0.520518947651184205 + mx * (
         0.609426039204995055 + mx * (
         0.724263522282908870 + mx * (
         0.871013847709812357 + mx * (
         1.057652872753547036)))))))))));
  } else if (m <= 0.4) {
    mx = m - 0.35;
    return 1.744350597225613243 + mx * (
         0.634864275371935304 + mx * (
         0.539842564164445538 + mx * (
         0.571892705193787391 + mx * (
         0.670295136265406100 + mx * (
         0.832586590010977199 + mx * (
         1.073857448247933265 + mx * (
         1.422091460675497751 + mx * (
         1.920387183402304829 + mx * (
         2.632552548331654201 + mx * (
         3.652109747319039160 + mx * (
         5.115867135558865806 + mx * (
         7.224080007363877411))))))))))));
  } else if (m <= 0.5) {
    mx = m - 0.45;
    return 1.813883936816982644 + mx * (
         0.763163245700557246 + mx * (
         0.761928605321595831 + mx * (
         0.951074653668427927 + mx * (
         1.315180671703161215 + mx * (
         1.928560693477410941 + mx * (
         2.937509342531378755 + mx * (
         4.594894405442878062 + mx * (
         7.330071221881720772 + mx * (
         11.87151259742530180 + mx * (
         19.45851374822937738 + mx * (
         32.20638657246426863 + mx * (
         53.73749198700554656 + mx * (
         90.27388602940998849)))))))))))));
  } else if (m <= 0.6) {
    mx = m - 0.55;
    return 1.898924910271553526 + mx * (
         0.950521794618244435 + mx * (
         1.151077589959015808 + mx * (
         1.750239106986300540 + mx * (
         2.952676812636875180 + mx * (
         5.285800396121450889 + mx * (
         9.832485716659979747 + mx * (
         18.78714868327559562 + mx * (
         36.61468615273698145 + mx * (
         72.45292395127771801 + mx * (
         145.1079577347069102 + mx * (
         293.4786396308497026 + mx * (
         598.3851815055010179 + mx * (
         1228.420013075863451 + mx * (
         2536.529755382764488))))))))))))));
  } else if (m <= 0.7) {
    mx = m - 0.65;
    return 2.007598398424376302 + mx * (
         1.248457231212347337 + mx * (
         1.926234657076479729 + mx * (
         3.751289640087587680 + mx * (
         8.119944554932045802 + mx * (
         18.66572130873555361 + mx * (
         44.60392484291437063 + mx * (
         109.5092054309498377 + mx * (
         274.2779548232413480 + mx * (
         697.5598008606326163 + mx * (
         1795.716014500247129 + mx * (
         4668.381716790389910 + mx * (
         12235.76246813664335 + mx * (
         32290.17809718320818 + mx * (
         85713.07608195964685 + mx * (
         228672.1890493117096 + mx * (
         612757.2711915852774))))))))))))))));
  } else if (m <= 0.8) {
    mx = m - 0.75;
    return 2.156515647499643235 + mx * (
         1.791805641849463243 + mx * (
         3.826751287465713147 + mx * (
         10.38672468363797208 + mx * (
         31.40331405468070290 + mx * (
         100.9237039498695416 + mx * (
         337.3268282632272897 + mx * (
         1158.707930567827917 + mx * (
         4060.990742193632092 + mx * (
         14454.00184034344795 + mx * (
         52076.66107599404803 + mx * (
         189493.6591462156887 + mx * (
         695184.5762413896145 + mx * (
         2.567994048255284686e6 + mx * (
         9.541921966748386322e6 + mx * (
         3.563492744218076174e7 + mx * (
         1.336692984612040871e8 + mx * (
         5.033521866866284541e8 + mx * (
         1.901975729538660119e9 + mx * (
         7.208915015330103756e9)))))))))))))))))));
  } else if (m <= 0.85) {
    mx = m - 0.825;
    return 2.318122621712510589 + mx * (
         2.616920150291232841 + mx * (
         7.897935075731355823 + mx * (
         30.50239715446672327 + mx * (
         131.4869365523528456 + mx * (
         602.9847637356491617 + mx * (
         2877.024617809972641 + mx * (
         14110.51991915180325 + mx * (
         70621.44088156540229 + mx * (
         358977.2665825309926 + mx * (
         1.847238263723971684e6 + mx * (
         9.600515416049214109e6 + mx * (
         5.030767708502366879e7 + mx * (
         2.654441886527127967e8 + mx * (
         1.408862325028702687e9 + mx * (
         7.515687935373774627e9)))))))))))))));
  } else {
    mx = m - 0.875;
    return 2.473596173751343912 + mx * (
         3.727624244118099310 + mx * (
         15.60739303554930496 + mx * (
         84.12850842805887747 + mx * (
         506.9818197040613935 + mx * (
         3252.277058145123644 + mx * (
         21713.24241957434256 + mx * (
         149037.0451890932766 + mx * (
         1.043999331089990839e6 + mx * (
         7.427974817042038995e6 + mx * (
         5.350383967558661151e7 + mx * (
         3.892498869948708474e8 + mx * (
         2.855288351100810619e9 + mx * (
         2.109007703876684053e10 + mx * (
         1.566998339477902014e11 + mx * (
         1.170222242422439893e12 + mx * (
         8.777948323668937971e12 + mx * (
         6.610124275248495041e13 + mx * (
         4.994880537133887989e14 + mx * (
         3.785974339724029920e15)))))))))))))))))));
  }
}

}  // namespace numerics
}  // namespace principia
