import fs from "node:fs";
import path from "node:path";
import {
  AlignmentType,
  Document,
  Footer,
  Header,
  HeadingLevel,
  ImportedXmlComponent,
  Packer,
  PageNumber,
  Paragraph,
  ShadingType,
  Table,
  TableCell,
  TableRow,
  TextRun,
  WidthType,
  convertInchesToTwip,
} from "docx";

const outputPath = process.argv[2];
if (!outputPath) {
  throw new Error("Usage: node create.js /absolute/path/output.docx");
}

const outputDir = path.dirname(outputPath);
const assetDir = path.join(outputDir, "assets");
fs.mkdirSync(assetDir, { recursive: true });

const T = String.raw;
const title = T`基于STM32F103C8T6的智能手表设计 — 系统设计文档`;
const palette = {
  dark: "263238",
  primary: "37474F",
  light: "78909C",
  border: "D8E0E3",
  fill: "EEF3F6",
};

const font = { name: "Times New Roman", eastAsia: "SimSun" };

const run = (text, options = {}) =>
  new TextRun({ text, font, size: 24, ...options });

const para = (children, options = {}) =>
  new Paragraph({
    spacing: { after: 160, line: 300 },
    ...options,
    children: Array.isArray(children) ? children : [children],
  });

const bodyPara = (text, options = {}) =>
  para(run(text), {
    indent: { firstLine: convertInchesToTwip(0.33) },
    ...options,
  });

const heading = (text, level = 1) =>
  para(run(text, { bold: true, size: level === 1 ? 30 : 26, color: palette.dark }), {
    heading: level === 1 ? HeadingLevel.HEADING_1 : HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 120 },
  });

const cell = (text, options = {}) =>
  new TableCell({
    children: [para(run(text))],
    margins: { top: 120, bottom: 120, left: 120, right: 120 },
    ...options,
  });

const xmlEscape = (value) =>
  String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");

const toc = (entries) => {
  const cached = entries
    .map(({ title: entryTitle, level, page }) => {
      const indent = Math.max(0, level - 1) * 360;
      return `<w:p>
        <w:pPr>
          <w:pStyle w:val="TOC${level}"/>
          <w:tabs><w:tab w:val="right" w:leader="dot" w:pos="9000"/></w:tabs>
          <w:ind w:left="${indent}"/>
        </w:pPr>
        <w:r><w:t>${xmlEscape(entryTitle)}</w:t></w:r>
        <w:r><w:tab/></w:r>
        <w:r><w:t>${xmlEscape(page)}</w:t></w:r>
      </w:p>`;
    })
    .join("");

  return ImportedXmlComponent.fromXmlString(`<w:sdt xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
    <w:sdtPr><w:alias w:val="目录"/></w:sdtPr>
    <w:sdtContent>
      <w:p>
        <w:r>
          <w:fldChar w:fldCharType="begin" w:dirty="true"/>
          <w:instrText xml:space="preserve"> TOC \\o &quot;1-3&quot; \\h \\z \\u </w:instrText>
          <w:fldChar w:fldCharType="separate"/>
        </w:r>
      </w:p>
      ${cached}
      <w:p><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>
    </w:sdtContent>
  </w:sdt>`).root[0];
};

const sections = [
  {
    title: T`一、物料到位情况`,
    level: 1,
    page: 3,
    paragraphs: [
      T`截至本报告提交日，本组物料到位情况如下：`,
    ],
    table: () => {
      const widths = [800, 3000, 800, 1600, 2400];
      const h = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[0], type: WidthType.DXA } });
      const h2 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[1], type: WidthType.DXA } });
      const h3 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[2], type: WidthType.DXA } });
      const h4 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[3], type: WidthType.DXA } });
      const h5 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[4], type: WidthType.DXA } });
      return new Table({
        width: { size: 100, type: WidthType.PERCENTAGE },
        columnWidths: widths,
        rows: [
          new TableRow({ children: [h("序号"), h2("器件名称"), h3("数量"), h4("到位状态"), h5("备注")] }),
          new TableRow({ children: [cell("1"), cell("STM32F103C8T6最小系统板"), cell("1"), cell("已到位"), cell("已有，含板载AMS1117-3.3V稳压")] }),
          new TableRow({ children: [cell("2"), cell("ST-Link V2调试器"), cell("1"), cell("已到位"), cell("已有")] }),
          new TableRow({ children: [cell("3"), cell("1.3寸 OLED（SSD1306，I2C，4针）"), cell("2"), cell("已采购/在途"), cell("预计7月1日到货，1用1备")] }),
          new TableRow({ children: [cell("4"), cell("GY-521 MPU6050模块"), cell("2"), cell("已采购/在途"), cell("预计7月1日到货，1用1备")] }),
          new TableRow({ children: [cell("5"), cell("TP4056 Type-C充电模块"), cell("1"), cell("已采购/在途"), cell("预计7月1日到货")] }),
          new TableRow({ children: [cell("6"), cell("602040 锂聚合物电池 3.7V 400mAh"), cell("2"), cell("已采购/在途"), cell("预计7月1日到货")] }),
          new TableRow({ children: [cell("7"), cell("EC11旋转编码器（带开关）"), cell("2"), cell("已采购/在途"), cell("预计7月1日到货，1用1备")] }),
          new TableRow({ children: [cell("8"), cell("HC-05蓝牙模块（带底板）"), cell("2"), cell("已采购/在途"), cell("预计7月1日到货，仅安卓支持，1用1备")] }),
          new TableRow({ children: [cell("9"), cell("1/4W直插电阻包（30种/600个）"), cell("1"), cell("已采购/在途"), cell("预计7月1日到货，含4.7K/10K/1K/220Ω")] }),
          new TableRow({ children: [cell("10"), cell("瓷片电容104（100nF，100个）"), cell("1"), cell("已采购/在途"), cell("预计7月1日到货")] }),
          new TableRow({ children: [cell("11"), cell("独石电容10uF（5个/份）"), cell("3"), cell("已采购/在途"), cell("预计7月1日到货，共15个")] }),
          new TableRow({ children: [cell("12"), cell("6×6×5mm直插轻触按键"), cell("1"), cell("已采购/在途"), cell("预计7月1日到货，通常1份50个")] }),
          new TableRow({ children: [cell("13"), cell("1N5819肖特基二极管（直插，50只）"), cell("1"), cell("已采购/在途"), cell("预计7月1日到货，电池防反接")] }),
          new TableRow({ children: [cell("14"), cell("CH340C USB转TTL串口模块"), cell("1"), cell("已采购/在途"), cell("预计7月1日到货，串口调试")] }),
          new TableRow({ children: [cell("15"), cell("5×7cm 洞洞板（2张/份）"), cell("2"), cell("已采购/在途"), cell("预计7月1日到货，共4张")] }),
          new TableRow({ children: [cell("16"), cell("示波器、万用表、面包板、跳线等"), cell("1"), cell("已到位"), cell("已有")] }),
          new TableRow({ children: [cell("17"), cell("18650电池、电烙铁、锡膏、无铅焊锡"), cell("1"), cell("已到位"), cell("已有，18650作为备用调试电源")] }),
        ],
      });
    },
  },
  {
    title: T`二、系统方案设计`,
    level: 1,
    page: 4,
    paragraphs: [
      T`2.1 总体架构`,
      T`系统采用STM32F103C8T6作为主控MCU，以FreeRTOS作为实时操作系统内核，实现多任务并行调度。系统架构分为四层：硬件层（MCU、传感器、显示、电源、通信）、驱动层（I2C、GPIO、UART、定时器、PWM）、中间件层（FreeRTOS任务调度、内存管理、任务间通信）和应用层（UI显示、时间管理、传感器采集、蓝牙通信、计步算法）。`,
      T`2.2 硬件框图`,
      T`硬件系统组成：`,
      T`• 主控：STM32F103C8T6（72MHz Cortex-M3，64KB Flash，20KB RAM）`,
      T`• 显示：1.3寸 OLED，I2C接口（SCL→PB6，SDA→PB7），SSD1306驱动，128×64分辨率`,
      T`• 传感器：MPU6050，I2C接口（与OLED共享I2C总线），地址0x68（AD0接地），提供3轴加速度+3轴陀螺仪数据`,
      T`• 电源：3.7V锂电池 → TP4056充电管理 → 系统板5V输入 → 板载AMS1117-3.3V稳压至3.3V → 系统供电`,
      T`• 输入：EC11旋转编码器（A相→PA0，B相→PA1，按键→PA2），用于菜单切换与选择`,
      T`• 通信：HC-05蓝牙模块（UART1：TX→PA9，RX→PA10），9600波特率，与安卓手机同步数据`,
      T`• 开关：直插轻触开关接系统板复位引脚或自定义GPIO，用于一键开机与菜单确认`,
      T`2.3 软件流程图`,
      T`软件系统启动流程：系统上电 → 初始化时钟/GPIO/外设 → 初始化I2C总线 → 初始化OLED与MPU6050 → 初始化FreeRTOS → 创建任务（UI任务、时间管理任务、传感器采集任务、蓝牙通信任务） → 启动调度器 → 各任务按优先级运行。`,
      T`主程序循环由FreeRTOS调度器管理，高优先级任务（如UI刷新）可抢占低优先级任务（如传感器数据采集）。任务间通过消息队列和信号量进行同步与数据传递。`,
    ],
  },
  {
    title: T`三、详细设计文档`,
    level: 1,
    page: 6,
    paragraphs: [
      T`3.1 硬件部分`,
      T`（1）I2C接口电路：OLED与MPU6050共享I2C1总线，SCL接PB6，SDA接PB7。I2C总线需外接上拉电阻（4.7KΩ）至3.3V。STM32F103C8T6的I2C1支持标准模式（100kHz）和快速模式（400kHz），本项目采用400kHz快速模式。`,
      T`（2）电源电路：3.7V锂电池（标称4.2V满电）经TP4056模块充电管理，输出直接接入STM32最小系统板的5V输入端，由系统板自带的AMS1117-3.3V稳压芯片降压至3.3V，供MCU及所有外设使用。TP4056输入为Type-C 5V，充电电流默认约1A。面包板调试阶段可直接通过USB线给系统板供电，系统板3.3V输出引脚为外设供电。电池供电时在TP4056输出端串联1N5819肖特基二极管，实现电池防反接保护。`,
      T`（3）一键开机电路：采用直插轻触开关直接接入系统板复位引脚（NRST）或自定义GPIO（如PA3）。按下开关时系统复位或触发GPIO中断，实现开机唤醒；配合软件逻辑，可实现长按关机功能。此方案简洁可靠，无需额外MOS管驱动电路。`,
      T`（4）旋转编码器接口：EC11的A、B相输出接PA0、PA1（配置为外部中断输入，上升沿/下降沿触发），按键接PA2（配置为GPIO输入，内部上拉）。通过中断服务程序检测A/B相的相位差判断旋转方向，按键用于菜单确认。`,
      T`（5）蓝牙模块接口：HC-05的TXD接PA10（USART1_RX），RXD接PA9（USART1_TX），EN接PA4（用于AT模式配置）。波特率默认9600，支持蓝牙串口透传。`,
      T`3.2 软件部分`,
      T`（1）模块划分：软件系统划分为以下模块：`,
      T`• bsp_i2c.c / bsp_i2c.h：I2C底层驱动，封装读写函数`,
      T`• bsp_oled.c / bsp_oled.h：SSD1306 OLED驱动，提供清屏、画点、画线、显示字符/字符串、显示数字等接口`,
      T`• bsp_mpu6050.c / bsp_mpu6050.h：MPU6050驱动，初始化DMP或直接读取原始加速度/陀螺仪数据`,
      T`• bsp_encoder.c / bsp_encoder.h：旋转编码器驱动，处理外部中断，提供旋转方向与按键事件`,
      T`• bsp_bluetooth.c / bsp_bluetooth.h：HC-05蓝牙串口驱动，封装数据发送/接收`,
      T`• app_ui.c / app_ui.h：UI显示任务，管理OLED页面（时间页面、传感器数据页面、菜单页面）`,
      T`• app_time.c / app_time.h：时间管理任务，基于SysTick或通用定时器产生秒中断，维护软件RTC`,
      T`• app_sensor.c / app_sensor.h：传感器采集任务，定期读取MPU6050数据，实现计步算法`,
      T`• app_bluetooth.c / app_bluetooth.h：蓝牙通信任务，处理数据同步与指令接收`,
      T`（2）关键算法描述：`,
      T`• 时间维护算法：使用STM32的通用定时器TIM2（或SysTick）配置为1秒中断。每次中断将软件RTC的秒计数器加1，并换算为时/分/秒格式。时间初始值可在菜单中设置。`,
      T`• 计步算法：基于MPU6050的加速度计Z轴数据，采用动态阈值法。检测加速度波形的波峰与波谷，当相邻波峰/波谷差值超过动态阈值且时间间隔在合理范围（0.2s~2s）内时，判定为有效步数。动态阈值根据最近若干步的峰值自适应更新。`,
      T`（3）外设配置方案：`,
      T`• I2C1：SCL=PB6，SDA=PB7，Fast-mode（400kHz），使用STM32 HAL库或寄存器直接操作`,
      T`• USART1：TX=PA9，RX=PA10，波特率9600，用于HC-05蓝牙通信`,
      T`• GPIO：PA0/PA1/PA2用于EC11编码器，PA3预留用于电源控制或按键中断，PA4用于HC-05 EN引脚`,
      T`• TIM2：配置为1秒定时中断，用于软件RTC`,
      T`（4）FreeRTOS任务划分：`,
      T`• Task_UI：优先级3（最高），周期100ms，负责OLED界面刷新与菜单响应。阻塞等待编码器事件队列或页面切换信号量。`,
      T`• Task_Time：优先级2，周期1000ms，负责维护软件RTC，更新时间数据结构。通过消息队列向Task_UI发送时间数据。`,
      T`• Task_Sensor：优先级2，周期50ms，负责读取MPU6050加速度数据，执行计步算法，将步数与姿态数据通过消息队列发送给Task_UI。`,
      T`• Task_Bluetooth：优先级1，事件驱动，监听USART1接收中断，解析蓝牙指令，将数据同步至手机或接收手机控制命令。`,
      T`任务间通信：使用FreeRTOS消息队列（Queue）传递传感器数据和步数；使用二值信号量（Semaphore）同步编码器中断事件；使用互斥量（Mutex）保护共享的OLED I2C总线访问。`,
    ],
  },
  {
    title: T`四、进度汇报`,
    level: 1,
    page: 9,
    paragraphs: [
      T`4.1 当前完成情况`,
      T`• 已完成：项目计划书编写、物料采购下单、系统方案初步设计、FreeRTOS源码下载与工程框架搭建。`,
      T`• 进行中：等待物料到货，准备硬件搭建；同时预研MPU6050 DMP库和OLED驱动代码。`,
      T`• 未开始：硬件焊接、软件驱动编写、多任务集成调试。`,
      T`4.2 后续工作计划`,
      T`• 7月1日：物料到货，完成面包板硬件搭建，点亮OLED，确认MPU6050 I2C通信正常。`,
      T`• 7月3日：完成FreeRTOS移植，创建各任务框架，实现时间显示与传感器数据读取。`,
      T`• 7月5日：实现旋转编码器菜单切换、蓝牙通信基本功能。`,
      T`• 7月6日：中期检查，提交中期检查报告，演示基本功能。`,
      T`• 7月7日~8日：实现计步算法、数据记录，完成洞洞板焊接与系统联调。`,
      T`• 7月9日：第一次验收，演示全部功能。`,
      T`• 7月9日晚~10日：根据教师意见修改，完成最终验收。`,
    ],
  },
];

const children = [
  para(run(title, { bold: true, size: 36, color: palette.dark }), {
    heading: HeadingLevel.TITLE,
    alignment: AlignmentType.CENTER,
    spacing: { after: 360 },
  }),
  para(run("目录", { bold: true, size: 30, color: palette.dark }), {
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 240, after: 120 },
  }),
  para(run(T`右键目录，选择"更新域"刷新页码。`, { italics: true, color: palette.light })),
  toc(
    sections.map(({ title: entryTitle, level, page }) => ({
      title: entryTitle,
      level,
      page,
    })),
  ),
];

for (const section of sections) {
  children.push(heading(section.title, section.level));
  for (const paragraph of section.paragraphs) {
    children.push(bodyPara(paragraph));
  }
  if (section.table) {
    children.push(section.table());
  }
}

const doc = new Document({
  features: { updateFields: true },
  sections: [
    {
      properties: {
        page: {
          margin: { top: 1440, bottom: 1440, left: 1440, right: 1440 },
        },
      },
      headers: {
        default: new Header({
          children: [
            para(run(title, { bold: true, color: palette.primary }), {
              alignment: AlignmentType.CENTER,
            }),
          ],
        }),
      },
      footers: {
        default: new Footer({
          children: [
            para(new TextRun({ children: [PageNumber.CURRENT] }), {
              alignment: AlignmentType.CENTER,
            }),
          ],
        }),
      },
      children,
    },
  ],
});

const buffer = await Packer.toBuffer(doc);
fs.writeFileSync(outputPath, buffer);
