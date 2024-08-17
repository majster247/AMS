// src/components/Home.tsx
import React from 'react';
import './Home.css';
const Home: React.FC = () => {
  return (
    <div>
      <div className="latest-info">
        <h3>Latest info:</h3>
        <img src="https://i.redd.it/jb0u633yc7v81.gif"></img>

        <pre>PL:<br></br>
          Po wielu miesiącach pracy nad systemem utworzyła sie klarowna wizja systemu. Wersja 1.0.0 jest w trakcie tworzenia.<br></br>
          Aktualna sytuacja pozwala mi przystąpić do pracy nad dziełem by zapewnić jak najlepsze wsparcie dla użytkowników oraz komfort i radość z użytkowania systemu.<br></br>
          <br></br>
          Wersja systemu 1.0.0 będzie zawierała:<br></br>
          - pełny system plików ShadowWizard<br></br>
          - pełny system bootowania Baobab<br></br>
          - wsparcie dla wielu architektur (planowane wsparcie 32bit i 64bit. W dalszym planie możliwe wsparcie dla 16bit po zoptymalizowaniu systemu)<br></br>
          - W pełni działający kernel czasu rzeczywistego z wsparciem dla wielu procesorów<br></br>
          - sterowniki VGA i sieci (w przyszłości planowane wsparcie dla HDMI). Planowane wsparcie dla rozdzielczości do 8k 30.0Hz<br></br>
          - wsparcie dla wielu języków programowania (w tym C, C++, ASM oraz własny język programowania Summerian)<br></br>
          - wsparcie dla wielu narzędzi deweloperskich (w tym własny kompilator Summerian z możliwościa personalizacji)<br></br>
          - wsparcie dla wielu aplikacji (w tym przeglądarka internetowa, odtwarzacz muzyki, edytor tekstu, narzędzia programistyczne, narzędzia graficzne)<br></br>
        <br></br>
          W planach na przyszłość znajduje się również uruchomienie odpowiedniego serwera i serwisu. Serwis pozwoli na łączenie ludzi zainteresowanych systemem oraz na rozwijanie systemu w odpowiednim kierunku.<br></br>
          Pozwoli wam odkryć Babilon widziany moimi oczami, przedstawi w zabawny i humorystyczny sposób wiele ciekawostek oraz informacji na temat systemu oraz pozwoli wam tworzyć własne elementy systemu dostępne dla wszystkich użytkowników.<br></br> 
          Serwis będzie obsługiwany w ramach ciekawego projektu, w ramach gry 2.5D dostępnej z poziomu przeglądarki internetowej oraz na systemie AMS-OS.<br></br>
          Logując się na serwis odkryjecie wiele ciekawych map, postaci, zadań oraz informacji. Dodatkowo każdy zalogowany użytkownik z poziomu AMS-OS dostanie pakiet bonusów.<br></br>
<br></br>
          Planem docelowym jest dystrybucja specjalnej wersji systemu w wersji fizycznej. W zestawie ma sie znaleźć płyta z systemem, książka z ciekawostkami oraz wiedzą odnośnie kompilatora. <br></br>
          Dodatkowo w pakiecie będziecie mogli uzyskać specjalną limitowaną wersję AMS-OS na dyskietce. <br></br>
          Wszystko to w ciekawym opakowaniu, które pozwoli wam odkryć Babilon widziany moimi oczami oczywiście :)<br></br>
<br></br>
          Bardzo zachęcam więc was do przeglądania informacji które w tym mozolnym procesie tworzenia systemu będą pojawiać się na stronie oraz na serwisie.<br></br>
          Wsparcie uzyskane za pomocą Patronite oraz Github Sponsors pozwoli mi na rozwijanie systemu oraz na tworzenie ciekawych treści dla was, użytkowników systemu.<br></br>
          Wspierając projekt nie zapomnijcie również o kontakcie do mnie, z pewnością wasze wsparcie pozowli wam na otrzymanie specjalnych bonusów oraz na rozwijanie systemu w odpowiednim kierunku.<br></br>
          <br></br>

          AMS-OS zawsze będzie stało otworem dla waszych pomysłów oraz sugestii. <br></br>
          Będzie to nowe miejsce w interncie które pozwoli wam zbudować cywilizację wirtualną w oparciu o świat który stworzę a później wy będziecie współtworzyć.<br></br>
          Ma to zachęcić ludzi do nauki przedmiotów ścisłych, kontaktów i kooperacji z ludzmi na całym świecie w dobie degeneracji internetowej jaką obserwujemy na co dzień.<br></br>
          Będzie to miejsce w którym każdy znajdzie coś dla siebie, od nauki programowania, poprzez naukę języków obcych, po naukę historii, geografii, matematyki oraz innych przedmiotów.<br></br> 
          Będzie to miejsce w którym każdy będzie mógł się rozwijać, uczyć oraz dzielić swoją wiedzą z innymi.<br></br>
          Będzie to miejsce wolne od zasad, reguł oraz ograniczeń gdyż będą one tworzone przez was, użytkowników systemu.<br></br>
          Będzie to miejsce w którym każdy będzie mógł się spełnić oraz znaleźć swoje miejsce na ziemi w wirtualnym świecie Babilonu.
          <br></br>Począwszy od budowy farmy, otworzeniu firmy i budowania świata na zlecenia graczy aż do przemysłu który będzie zaopatrywał cały świat w surowce.<br></br>
          Będzie to miejsce w końcu wolne od ścieku internetowego, szamba intelektualnego prezentowanego na co dzień w internecie.<br></br>
          Będzie to miejsce dla was.<br></br>
    <br></br>
          Jeszcze raz dziękuje wam za wsparcie oraz za zainteresowanie systemem. Pozostaję w kontakcie z każdym kto będzie chciał pomóc w rozwoju systemu oraz w tworzeniu ciekawych treści.<br></br>
          Niech ta wiadomość wybrzmi w świat i rozgłosi nowinę. Babilon powstaje i będziecie mogli go odkryć już wkrótce.<br></br>
<br></br>
          ~ Majster247     <br></br><br></br><br></br>
          </pre>
          <pre>
          EN:<br></br>
After many months of work on the system, a clear vision of the system has been created. Version 1.0.0 is in the process of being created.<br></br>
The current situation allows me to start working on the work to provide the best possible support for users and comfort and joy of using the system.<br></br>
<br></br>
System version 1.0.0 will include:<br></br>
- full ShadowWizard file system<br></br>
- full Baobab boot system<br></br>
- support for many architectures (planned support for 32bit and 64bit. In the future, possible support for 16bit after optimizing the system)<br></br>
- Fully working real-time kernel with support for many processors<br></br>
- VGA and network drivers (planned support for HDMI in the future). Planned support for resolutions up to 8k 30.0Hz<br></br>
- support for many programming languages ​​(including C, C++, ASM and Summerian's own programming language)<br></br>
- support for many development tools (including Summerian's own compiler with personalization)<br></br>
- support for many applications (including a web browser, music player, text editor, programming tools, graphic tools)<br></br>
<br></br>
Future plans also include launching an appropriate server and service. The service will allow you to connect people interested in the system and develop the system in the right direction.<br></br>
It will allow you to discover Babylon seen through my eyes, present many interesting facts and information about the system in a funny and humorous way and allow you to create your own system elements available to all users.<br></br>
The service will be operated as part of an interesting project, as part of a 2.5D game available from a web browser and on the AMS-OS system.<br></br>
By logging in to the service, you will discover many interesting maps, characters, tasks and information. <br></br>
Additionally, each user logged in from AMS-OS will receive a bonus package.<br></br>
<br></br>
The final plan is to distribute a special version of the system in a physical version. The set will include a CD with the system, a book with interesting facts and knowledge about the compiler. <br></br>
Additionally, in the package you will be able to get a special limited version of AMS-OS on a floppy disk.<br></br>
All this in an interesting packaging that will allow you to discover Babylon seen through my eyes, of course :)<br></br>
<br></br>
I strongly encourage you to browse the information that will appear on the website and on the service during this laborious process of creating the system.<br></br>
Support obtained through Patronite and Github Sponsors will allow me to develop the system and create interesting content for you, the users of the system.<br></br>
When supporting the project, do not forget to contact me, your support will certainly allow you to receive special bonuses and develop the system in the right direction.<br></br>
<br></br>
AMS-OS will always be open to your ideas and suggestions. It will be a new place on the Internet that will allow you to build a virtual civilization based on the world that I will create and then you will co-create.<br></br>
This is to encourage people to learn science, contact and cooperate with people around the world in the era of internet degeneration that we observe every day.<br></br>
It will be a place where everyone will find something for themselves, from learning programming, through learning foreign languages, to learning history, geography, mathematics and other subjects.<br></br>
It will be a place where everyone will be able to develop, learn and share their knowledge with others.<br></br>
It will be a place free of rules, regulations and restrictions because they will be created by you, the users of the system.<br></br>
It will be a place where everyone will be able to fulfill themselves and find their place on earth in the virtual world of Babylon.<br></br>Starting from building a farm, opening a company and building a world on behalf of players, to an industry that will supply the whole world with raw materials.<br></br>
It will finally be a place free from the internet sewage, the intellectual cesspool presented on the internet every day.<br></br>
It will be a place for you.<br></br>
<br></br>
Thank you once again for your support and interest in the system. I will stay in touch with anyone who wants to help develop the system and create interesting content.<br></br>
Let this message resonate and spread the word. Babylon is rising and you will be able to discover it soon.<br></br>
<br></br>
~ Majster247<br></br><br></br><br></br>
          </pre>
      </div>




      <h2>About AMS-OS</h2>
      <p>
        AMS-OS is a simple and lightweight operating system designed for enthusiasts and developers.<br></br> 
        It's not like another Unix-based system - it's fully written by hand and compiled with its own compiler in its own language.
      </p>
      <h5>Github repo: <a href="https://github.com/majster247/AMS/">AMS-OS official repo</a></h5>


      <h2>Key Features</h2>
      <ul>
        <li>Real-time kernel</li>
        <li>Own filesystem called "ShadowWizard" and bootloader called "Baobab"</li>
        <li>Unique in a world of technology</li>
        <li>16/32/64-bit port (supporting multiple architectures)</li>
        <li>Network & VGA driver (up to 8k 30.0Hz support in pre-alpha, HDMI drivers in the future)</li>
      </ul>

      <br /><br />
      <p>PROJECT SPONSOR:</p>
      <p>Github Sponsor: <iframe src="https://github.com/sponsors/majster247/button" title="Sponsor majster247" height="32" width="114" style={{ border: 0, borderRadius: '6px' }}></iframe></p>
      <p>Patronite: ....</p>
      <p><b>In any issues, problems, or questions in supporting me, write to any e-mail in a contact section</b></p>
    </div>
  );
};

export default Home;
