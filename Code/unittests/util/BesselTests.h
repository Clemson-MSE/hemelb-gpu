
// This file is part of HemeLB and is Copyright (C)
// the HemeLB team and/or their institutions, as detailed in the
// file AUTHORS. This software is provided under the terms of the
// license in the file LICENSE.

#ifndef HEMELB_UNITTESTS_UTIL_BESSELTESTS_H
#define HEMELB_UNITTESTS_UTIL_BESSELTESTS_H

#include "util/Bessel.h"

namespace hemelb
{
  namespace unittests
  {
    namespace util
    {
      using namespace hemelb::util;

      class BesselTests : public CppUnit::TestFixture
      {
          CPPUNIT_TEST_SUITE( BesselTests);
          CPPUNIT_TEST( TestUsedRange);CPPUNIT_TEST_SUITE_END();

        public:
          typedef std::complex<double> Complex;

          void TestUsedRange()
          {
            /*
             * All the arguments for J0 we will be using for the Womersley flow
             * are of the form "x * i^(3/2)", with x elem Reals and x >= 0,
             * x ~< 20.
             *
             * The data here is generated with Mathematica using the expression
             * CForm[Table[BesselJ[0, I^(3/2) x], {x, 0, 20, 0.1}]]
             */
            int n = 201;
            Complex mathematica[] = { Complex(1., 0.),
                                      Complex(0.9999984375000678, 0.0024999995659722293),
                                      Complex(0.9999750000173611, 0.009999972222229167),
                                      Complex(0.9998734379449461, 0.022499683594150457),
                                      Complex(0.9996000044444366, 0.03999822222933332),
                                      Complex(0.9990234639908382, 0.06249321838219946),
                                      Complex(0.9979751139052248, 0.08997975041006064),
                                      Complex(0.9962488284440701, 0.12244893898161383),
                                      Complex(0.9936011377454146, 0.15988622950389433),
                                      Complex(0.989751356659594, 0.20226936348947042),
                                      Complex(0.9843817812130872, 0.24956604003665972),
                                      Complex(0.9771379731639945, 0.3017312692062659),
                                      Complex(0.9676291558011337, 0.3587044198731508),
                                      Complex(0.955428746808401, 0.42040596563410026),
                                      Complex(0.940075056652725, 0.48673393358890815),
                                      Complex(0.921072183546256, 0.5575600623030867),
                                      Complex(0.8978911385677055, 0.6327256770313981),
                                      Complex(0.8699712369877577, 0.7120372923542193),
                                      Complex(0.8367217942101612, 0.7952619547756586),
                                      Complex(0.7975241669915222, 0.8821223405745098),
                                      Complex(0.7517341827138088, 0.9722916273066614),
                                      Complex(0.6986850014256358, 1.0653881608492868),
                                      Complex(0.637690457109553, 1.1609699437702223),
                                      Complex(0.5680489261370965, 1.258528975115817),
                                      Complex(0.4890477721018263, 1.3574854764502742),
                                      Complex(0.39996841712953185, 1.4571820441598051),
                                      Complex(0.30009209030678824, 1.556877773663312),
                                      Complex(0.18870630399260907, 1.6557424072520857),
                                      Complex(0.06511210842734699, 1.7528505638144392),
                                      Complex(-0.07136782583144546, 1.8471761156832547),
                                      Complex(-0.221380249598693, 1.9375867852660433),
                                      Complex(-0.3855314549772813, 2.022839041963735),
                                      Complex(-0.564376430484566, 2.101573388135252),
                                      Complex(-0.7584070120727846, 2.1723101314924627),
                                      Complex(-0.968038995314976, 2.233445750279042),
                                      Complex(-1.1935981795899278, 2.283249966853916),
                                      Complex(-1.4353053217188478, 2.319863654812666),
                                      Complex(-1.6932599842696001, 2.3412977144765446),
                                      Complex(-1.9674232727394207, 2.345433061385532),
                                      Complex(-2.2575994661429886, 2.3300218822650764),
                                      Complex(-2.5634165572585808, 2.2926903226993027),
                                      Complex(-2.884305732008853, 2.2309427803269686),
                                      Complex(-3.219479832260941, 2.1421679866570256),
                                      Complex(-3.567910862806223, 2.0236470694401754),
                                      Complex(-3.9283066215020925, 1.8725637957779573),
                                      Complex(-4.29908655159976, 1.686017203632144),
                                      Complex(-4.678356937208986, 1.4610368359280403),
                                      Complex(-5.063885586719507, 1.1946007968223067),
                                      Complex(-5.453076174855464, 0.8836568537071567),
                                      Complex(-5.842942441915635, 0.52514681090883),
                                      Complex(-6.230082478666365, 0.11603438155020307),
                                      Complex(-6.610653357304579, -0.34666321759124497),
                                      Complex(-6.980346402874884, -0.8658397274844293),
                                      Complex(-7.3343634354629685, -1.4442601506049233),
                                      Complex(-7.667394351327405, -2.0845166930936614),
                                      Complex(-7.973596450774425, -2.7889801547340602),
                                      Complex(-8.246575961893132, -3.5597465933557295),
                                      Complex(-8.479372252085216, -4.398579111649334),
                                      Complex(-8.664445263435923, -5.306844640335233),
                                      Complex(-8.793666753132394, -6.285445622573309),
                                      Complex(-8.85831596604505, -7.334746540847954),
                                      Complex(-8.849080412899339, -8.454495269728723),
                                      Complex(-8.756062473778371, -9.643739286319303),
                                      Complex(-8.568792592550325, -10.900736825284714),
                                      Complex(-8.276249872685968, -12.22286312750625),
                                      Complex(-7.8668909282330635, -13.606512001062601),
                                      Complex(-7.328687884785503, -15.04699299076165),
                                      Complex(-6.649176463396302, -16.538424538220475),
                                      Complex(-5.815515114723403, -18.07362360884783),
                                      Complex(-4.814556200379463, -19.643992365325783),
                                      Complex(-3.6329302425079186, -21.239402579572218),
                                      Complex(-2.2571442799742227, -22.848078596893444),
                                      Complex(-0.6736953790976818, -24.456479797244924),
                                      Complex(1.130799652671509, -26.04918363926931),
                                      Complex(3.1694573116236713, -27.608770523048474),
                                      Complex(5.454962184384319, -29.115711867168),
                                      Complex(7.999382494360611, -30.54826296450837),
                                      Complex(10.813965475974232, -31.88236235878625),
                                      Complex(13.908911710925443, -33.09153966976509),
                                      Complex(17.293127645277128, -34.146833988573626),
                                      Complex(20.97395561073023, -35.016725164881585),
                                      Complex(24.956880799933536, -35.6670805137552),
                                      Complex(29.24521479594635, -36.06111968061995),
                                      Complex(33.83975543199855, -36.15940061643599),
                                      Complex(38.738422961436164, -35.91982983023938),
                                      Complex(43.93587275118549, -35.2977003006595),
                                      Complex(49.42308497717934, -34.245760639642114),
                                      Complex(55.18693209890024, -32.714319307847894),
                                      Complex(61.20972522438379, -30.651387879200858),
                                      Complex(67.46874084848511, -28.00286753863363),
                                      Complex(73.93572985761627, -24.712783168678463),
                                      Complex(80.57641114504037, -20.723569533273743),
                                      Complex(87.34995267350574, -15.976414196712359),
                                      Complex(94.20844335762223, -10.411661917365237),
                                      Complex(101.09635971777905, -3.9692853246023763),
                                      Complex(107.95003188108015, 3.410573282289649),
                                      Complex(114.69711417290264, 11.786984188327532),
                                      Complex(121.25606625495762, 21.217531809872916),
                                      Complex(127.53565152140547, 31.75753089606796),
                                      Complex(133.4344602623171, 43.459152932583784),
                                      Complex(138.84046594163306, 56.370458553906644),
                                      Complex(143.63062381213715, 70.5343321085276),
                                      Complex(147.6705219994189, 85.98731494758498),
                                      Complex(150.81409612611736, 102.75833453025177),
                                      Complex(152.9034195117527, 120.86732706724278),
                                      Complex(153.76858196600085, 140.32375216623788),
                                      Complex(153.22767118707793, 161.12499880891917),
                                      Complex(151.0868717735614, 183.25468298791785),
                                      Complex(147.14069784776854, 206.68083847254292),
                                      Complex(141.17237626071082, 231.3540034637578),
                                      Complex(132.95439829016493, 257.2052073505192),
                                      Complex(122.24925864054501, 284.1438633998555),
                                      Complex(108.81040139045513, 312.0555750100623),
                                      Complex(92.38339329376237, 340.79986513779556),
                                      Complex(72.70734550379775, 370.2078406822411),
                                      Complex(49.51660533700796, 400.07980597898813),
                                      Complex(22.542740099470723, 430.1828421271531),
                                      Complex(-8.483164757471364, 460.2483716491838),
                                      Complex(-43.8278708340345, 489.96973096520765),
                                      Complex(-83.75299317969774, 518.9997763525804),
                                      Complex(-128.5116261565375, 546.9485524542472),
                                      Complex(-178.34456773079762, 573.381055991806),
                                      Complex(-233.47612216880464, 597.8151311233315),
                                      Complex(-294.1094624784331, 619.7195368517118),
                                      Complex(-360.421535691228, 638.5122310222873),
                                      Complex(-432.5574962623391, 653.5589197316438),
                                      Complex(-510.62465551565, 664.171925380505),
                                      Complex(-594.6859382216367, 669.6094311165878),
                                      Complex(-684.7528411077179, 669.0751639969601),
                                      Complex(-780.7778924081615, 661.7185838172607),
                                      Complex(-882.6466165061374, 646.6356491651416),
                                      Complex(-990.169013347062, 622.8702368090755),
                                      Complex(-1103.0705686523079, 589.416294976599),
                                      Complex(-1220.982818076606, 545.2208153463855),
                                      Complex(-1343.4334963702809, 489.18771260749924),
                                      Complex(-1469.836311365625, 420.18270414991304),
                                      Complex(-1599.4803922387027, 337.0392857587834),
                                      Complex(-1731.5194720331976, 238.56590199767473),
                                      Complex(-1864.9608758961247, 123.5544121820599),
                                      Complex(-1998.6543988851104, -9.210045646563572),
                                      Complex(-2131.2811705748813, -160.93768965248415),
                                      Complex(-2261.3426180206816, -332.8211252212117),
                                      Complex(-2387.1496539221325, -526.020670598572),
                                      Complex(-2506.8122330569727, -741.6478782931026),
                                      Complex(-2618.229437190627, -980.7471573731195),
                                      Complex(-2719.0802666746513, -1244.275407337937),
                                      Complex(-2806.815335762566, -1533.0795815144097),
                                      Complex(-2878.649688232603, -1847.8721071122177),
                                      Complex(-2931.556970104238, -2189.2041003418817),
                                      Complex(-2962.2652169747125, -2557.436328547621),
                                      Complex(-2967.254534635083, -2952.7078873441424),
                                      Complex(-2942.7569730009754, -3374.90257945063),
                                      Complex(-2884.7589148252564, -3823.61300350275),
                                      Complex(-2789.006321935987, -4298.102385789525),
                                      Complex(-2651.013202623024, -4797.264215805336),
                                      Complex(-2466.0736840064974, -5319.579777924271),
                                      Complex(-2229.278092455104, -5863.073706580153),
                                      Complex(-1935.5334630396744, -6425.2677312486085),
                                      Complex(-1579.5889152317668, -7003.132820442227),
                                      Complex(-1156.0663461690638, -7593.039980989768),
                                      Complex(-659.4969043585443, -8190.710020205632),
                                      Complex(-84.36371517405723, -8791.162634261618),
                                      Complex(574.8486656070427, -9388.665246222208),
                                      Complex(1323.5975932740457, -9976.682081830399),
                                      Complex(2167.2179977958976, -10547.824040224878),
                                      Complex(3110.849906267163, -11093.799990280928),
                                      Complex(4159.357376599697, -11605.370201083806),
                                      Complex(5317.238417509861, -12072.30269700138),
                                      Complex(6588.525500186585, -12483.333413685452),
                                      Complex(7976.676305879186, -12826.131120785723),
                                      Complex(9484.454401869372, -13087.268169819397),
                                      Complex(11113.79959677058, -13252.198221012677),
                                      Complex(12865.687795735825, -13305.24220043798),
                                      Complex(14739.980257879633, -13229.583837722743),
                                      Complex(16735.26225296128, -13007.276234187166),
                                      Complex(18848.67122308173, -12619.26101055499),
                                      Complex(21075.714678745808, -12045.401681294392),
                                      Complex(23410.078198047544, -11264.532997983835),
                                      Complex(25843.42405385698, -10254.528095483736),
                                      Complex(28365.18116754182, -8992.385360621116),
                                      Complex(30962.327279773788, -7454.33702184651),
                                      Complex(33619.16444003406, -5615.981528055889),
                                      Complex(36317.089147197046, -3452.441843414259),
                                      Complex(39034.35872454733, -938.5518303392419),
                                      Complex(41745.855784149724, 1950.927077614604),
                                      Complex(44422.85292788767, 5241.056700049161),
                                      Complex(47032.78014575373, 8956.438228272344),
                                      Complex(49538.99770594729, 13120.891469429493),
                                      Complex(51900.577685633034, 17757.095109224694),
                                      Complex(54072.09766513732, 22886.185978780777),
                                      Complex(56003.45050097388, 28527.315438177833),
                                      Complex(57639.67450305501, 34697.16115441406),
                                      Complex(58920.808767105504, 41409.392758883696),
                                      Complex(59781.77885254478, 48674.090123030226),
                                      Complex(60152.31844641239, 56497.113294810995),
                                      Complex(59956.93311223414, 64879.42349727955),
                                      Complex(59114.912685530086, 73816.35500833792),
                                      Complex(57540.3993407745, 83296.8382218868),
                                      Complex(55142.518813337985, 93302.57473963358),
                                      Complex(51825.58270880667, 103807.1659639928),
                                      Complex(47489.37026506239, 114775.19736006652) };

            const Complex i = Complex(0, 1);
            const Complex iPowThreeHalves = pow(i, 1.5);
            const double epsilon = 1e-6;

            for (int i = 0; i < n; ++i)
            {
              Complex z = iPowThreeHalves * (i / 10.);
              Complex cAns = BesselJ0ComplexArgument(z);
              Complex& mathAns = mathematica[i];

              CPPUNIT_ASSERT_DOUBLES_EQUAL(mathAns.real(), cAns.real(), epsilon);
              CPPUNIT_ASSERT_DOUBLES_EQUAL(mathAns.imag(), cAns.imag(), epsilon);
            }
          }
      };

      CPPUNIT_TEST_SUITE_REGISTRATION(BesselTests);

    }
  }
}
#endif // HEMELB_UNITTESTS_UTIL_BESSELTESTS_H
