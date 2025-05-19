import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.*;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

@WebServlet({"/webTest", "/webTest/"})
public class mainServlet extends viewBaseServlet {
    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        HttpSession session = req.getSession();

        System.out.println("/webTest");
        System.out.println("当前Session ID:"+session.getId());
        System.out.println("当前备忘录列表："+session.getAttribute("allContents"));
        System.out.println("请求Cookie:"+req.getHeader("Cookie"));
        System.out.println("*********************************************");
        resp.setHeader("Access-Control-Allow-Origin", "http://212.129.223.4:8080");
        resp.setHeader("Access-Control-Allow-Credentials", "true");
        resp.setHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
        resp.setHeader("Access-Control-Allow-Headers", "Content-Type");

        List<String> allContents = (List<String>) session.getAttribute("allContents");
        if (allContents == null) allContents = new ArrayList<>();
        req.setAttribute("allContents", allContents);
        super.processTemplate("index", req, resp); // index.html 由模板引擎动态渲染
    }
}
